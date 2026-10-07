/*
 * FlintRTOS - coreMQTT transport over lwIP raw TCP (NO_SYS mode).
 *
 * lwIP's raw API is callback driven and, in NO_SYS mode, only makes progress
 * when the owning task polls the netif and runs sys_check_timeouts(). coreMQTT
 * expects a blocking-ish transport (MQTT_Connect waits for CONNACK, sends retry
 * on a full window). So whenever this transport has to wait, it calls
 * fm_plat_pump(), which runs one iteration of the network loop. Received data
 * is copied by the tcp_recv callback into a ring buffer that
 * mqtt_transport_recv() drains; lwIP's flow control is respected by refusing
 * (ERR_MEM) a pbuf the ring can't hold yet - lwIP re-delivers it later.
 */
#include "mqtt_transport.h"
#include "mqtt_platform.h"

#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"

#define RX_RING_SIZE   (4096U)

typedef enum
{
    TS_IDLE = 0,
    TS_RESOLVING,
    TS_CONNECTING,
    TS_UP,
    TS_CLOSED,
    TS_ERROR
} TransportState;

struct NetworkContext
{
    struct tcp_pcb   *pcb;
    volatile TransportState state;
    volatile bool     dns_done;
    volatile bool     dns_ok;
    ip_addr_t         addr;
    const char       *err;
    uint8_t           rx[RX_RING_SIZE];
    volatile uint32_t rx_head;          /* write index (callback) */
    volatile uint32_t rx_tail;          /* read index (MQTT)      */
};

static NetworkContext_t s_ctx;

NetworkContext_t *mqtt_transport_ctx(void) { return &s_ctx; }

static uint32_t ring_used(const NetworkContext_t *c) { return c->rx_head - c->rx_tail; }
static uint32_t ring_free(const NetworkContext_t *c) { return RX_RING_SIZE - ring_used(c); }

/* ---- lwIP callbacks (run inside fm_plat_pump) ----------------------------- */
static err_t on_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    NetworkContext_t *c = (NetworkContext_t *)arg;
    (void)err;

    if (p == NULL)
    {
        c->state = TS_CLOSED;              /* peer closed the connection */
        c->err   = "closed by broker";
        return ERR_OK;
    }
    if (p->tot_len > ring_free(c))
    {
        return ERR_MEM;                    /* lwIP keeps it and retries later */
    }
    for (struct pbuf *q = p; q != NULL; q = q->next)
    {
        const uint8_t *src = (const uint8_t *)q->payload;
        for (u16_t i = 0U; i < q->len; i++)
        {
            c->rx[c->rx_head % RX_RING_SIZE] = src[i];
            c->rx_head++;
        }
    }
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static void on_err(void *arg, err_t err)
{
    NetworkContext_t *c = (NetworkContext_t *)arg;
    (void)err;
    c->pcb   = NULL;                       /* lwIP already freed it */
    c->state = TS_ERROR;
    c->err   = (err == ERR_RST) ? "reset by peer" :
               (err == ERR_ABRT) ? "aborted" : "TCP error / timeout";
}

static err_t on_connected(void *arg, struct tcp_pcb *pcb, err_t err)
{
    NetworkContext_t *c = (NetworkContext_t *)arg;
    (void)pcb;
    c->state = (err == ERR_OK) ? TS_UP : TS_ERROR;
    return ERR_OK;
}

static void on_dns(const char *name, const ip_addr_t *ip, void *arg)
{
    NetworkContext_t *c = (NetworkContext_t *)arg;
    (void)name;
    if (ip != NULL) { c->addr = *ip; c->dns_ok = true; }
    c->dns_done = true;
}

/* Pump the stack until cond() or timeout. */
static bool wait_for(NetworkContext_t *c, bool (*cond)(const NetworkContext_t *), uint32_t timeout_ms)
{
    uint32_t t0 = fm_plat_ms();
    while (!cond(c))
    {
        if ((uint32_t)(fm_plat_ms() - t0) >= timeout_ms) { return false; }
        fm_plat_pump(true);
    }
    return true;
}
static bool dns_finished(const NetworkContext_t *c) { return c->dns_done; }
static bool tcp_settled(const NetworkContext_t *c)  { return c->state != TS_CONNECTING; }

/* ---- API ------------------------------------------------------------------- */
int mqtt_transport_open(NetworkContext_t *c, const char *host, uint16_t port, uint32_t timeout_ms)
{
    err_t e;

    mqtt_transport_close(c);
    c->rx_head = 0U; c->rx_tail = 0U;
    c->err = NULL;

    /* 1. Resolve (dotted quads resolve immediately). */
    c->dns_done = false; c->dns_ok = false;
    c->state = TS_RESOLVING;
    e = dns_gethostbyname(host, &c->addr, on_dns, c);
    if (e == ERR_OK)
    {
        c->dns_ok = true; c->dns_done = true;
    }
    else if (e == ERR_INPROGRESS)
    {
        if (!wait_for(c, dns_finished, timeout_ms)) { c->err = "DNS timeout"; c->state = TS_ERROR; return -1; }
    }
    else
    {
        c->err = "DNS request failed (no DNS server?)"; c->state = TS_ERROR; return -1;
    }
    if (!c->dns_ok) { c->err = "DNS: host not found"; c->state = TS_ERROR; return -1; }

    /* 2. TCP connect. */
    c->pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
    if (c->pcb == NULL) { c->err = "out of TCP PCBs"; c->state = TS_ERROR; return -1; }
    tcp_arg(c->pcb, c);
    tcp_recv(c->pcb, on_recv);
    tcp_err(c->pcb, on_err);
    tcp_nagle_disable(c->pcb);            /* MQTT packets are small: no 200 ms coalescing */
    c->state = TS_CONNECTING;
    e = tcp_connect(c->pcb, &c->addr, port, on_connected);
    if (e != ERR_OK) { c->err = "tcp_connect failed"; mqtt_transport_close(c); c->state = TS_ERROR; return -1; }

    if (!wait_for(c, tcp_settled, timeout_ms))
    {
        c->err = "TCP connect timeout";
        mqtt_transport_close(c);
        c->state = TS_ERROR;
        return -1;
    }
    if (c->state != TS_UP)
    {
        if (c->err == NULL) { c->err = "connection refused"; }
        mqtt_transport_close(c);
        c->state = TS_ERROR;
        return -1;
    }
    /* TCP keepalive catches a silently dead path between MQTT pings. */
    c->pcb->so_options |= SOF_KEEPALIVE;
    return 0;
}

void mqtt_transport_close(NetworkContext_t *c)
{
    if (c->pcb != NULL)
    {
        tcp_arg(c->pcb, NULL);
        tcp_recv(c->pcb, NULL);
        tcp_err(c->pcb, NULL);
        if (tcp_close(c->pcb) != ERR_OK) { tcp_abort(c->pcb); }
        c->pcb = NULL;
    }
    if (c->state != TS_ERROR) { c->state = TS_IDLE; }
}

bool mqtt_transport_is_up(const NetworkContext_t *c)
{
    return (c->state == TS_UP) && (c->pcb != NULL);
}

const char *mqtt_transport_error(const NetworkContext_t *c)
{
    return (c->err != NULL) ? c->err : "none";
}

int32_t mqtt_transport_send(NetworkContext_t *c, const void *buf, size_t len)
{
    u16_t room;
    u16_t n;

    if (!mqtt_transport_is_up(c)) { return -1; }

    room = tcp_sndbuf(c->pcb);
    if ((room == 0U) || (tcp_sndqueuelen(c->pcb) >= (TCP_SND_QUEUELEN - 1)))
    {
        fm_plat_pump(true);                /* let ACKs free the window */
        return 0;
    }
    n = (len > room) ? room : (u16_t)len;
    if (tcp_write(c->pcb, buf, n, TCP_WRITE_FLAG_COPY) != ERR_OK)
    {
        fm_plat_pump(true);
        return 0;
    }
    (void)tcp_output(c->pcb);
    return (int32_t)n;
}

int32_t mqtt_transport_recv(NetworkContext_t *c, void *buf, size_t len)
{
    uint8_t *dst = (uint8_t *)buf;
    uint32_t n = 0U;

    if (ring_used(c) == 0U)
    {
        fm_plat_pump(false);               /* maybe something just arrived */
        if (ring_used(c) == 0U)
        {
            return mqtt_transport_is_up(c) ? 0 : -1;
        }
    }
    while ((n < len) && (ring_used(c) > 0U))
    {
        dst[n] = c->rx[c->rx_tail % RX_RING_SIZE];
        c->rx_tail++;
        n++;
    }
    return (int32_t)n;
}
