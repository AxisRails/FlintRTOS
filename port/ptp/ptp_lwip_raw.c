/*
 * FlintRTOS - PTP slave over lwIP raw UDP (NO_SYS). See ptp_lwip_raw.h.
 *
 * Timestamp path (why this only works in NO_SYS mode): flint_netif_poll()
 * pulls a frame + its driver RX stamp out of GENET and calls netif->input(),
 * which runs ip4_input -> udp_input -> rx_cb() synchronously, so rx_cb can ask
 * flint_netif_rx_stamp() for the stamp of exactly this frame. Likewise
 * udp_sendto() runs down to genet_send() before returning, so the TX stamp of
 * the Delay_Req is flint_netif_tx_stamp() right afterwards.
 */
#include "ptp_lwip_raw.h"
#include "ptp_core.h"
#include "ptp_config.h"
#include "flint_netif.h"
#include "uart.h"

#include "lwip/udp.h"
#include "lwip/igmp.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"

#define PTP_MAX_MSG   (128U)

static struct udp_pcb *s_ev;
static struct udp_pcb *s_gen;
static ip_addr_t       s_mcast;
static bool            s_started;

static void rx_cb(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                  const ip_addr_t *addr, u16_t port)
{
    uint8_t  buf[PTP_MAX_MSG];
    uint16_t len;
    (void)pcb; (void)addr; (void)port;

    if (p == NULL) { return; }
    len = pbuf_copy_partial(p, buf, (u16_t)sizeof(buf), 0U);
    ptp_core_rx(buf, len, (uint16_t)(uintptr_t)arg, flint_netif_rx_stamp());
    pbuf_free(p);
}

static bool tx_fn(uint16_t port, const uint8_t *buf, uint16_t len, uint64_t *tx_cnt)
{
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    err_t err;
    if (p == NULL) { return false; }
    (void)pbuf_take(p, buf, len);
    err = udp_sendto((port == (uint16_t)PTP_EVENT_PORT) ? s_ev : s_gen, p, &s_mcast, port);
    *tx_cnt = flint_netif_tx_stamp();          /* genet_send() already ran */
    pbuf_free(p);
    return (err == ERR_OK);
}

static struct udp_pcb *open_port(uint16_t port)
{
    struct udp_pcb *pcb = udp_new();
    if (pcb == NULL) { return NULL; }
    if (udp_bind(pcb, IP4_ADDR_ANY, port) != ERR_OK)
    {
        udp_remove(pcb);
        return NULL;
    }
    udp_set_multicast_ttl(pcb, 1U);            /* PTP stays on the link */
    udp_recv(pcb, rx_cb, (void *)(uintptr_t)port);
    return pcb;
}

bool ptp_raw_start(struct netif *nif)
{
    err_t ig;

    if (s_started) { return true; }
    ipaddr_aton(PTP_MCAST_ADDR, &s_mcast);

    s_ev  = open_port((uint16_t)PTP_EVENT_PORT);
    s_gen = open_port((uint16_t)PTP_GENERAL_PORT);
    if ((s_ev == NULL) || (s_gen == NULL))
    {
        uart_printf("[ptp] UDP 319/320 bind failed\n");
        return false;
    }

    ig = igmp_joingroup_netif(nif, ip_2_ip4(&s_mcast));
    uart_printf("[ptp] UDP 319/320 open, IGMP join %s -> %s\n", PTP_MCAST_ADDR,
                (ig == ERR_OK) ? "ok" : "FAILED");

    ptp_core_init(nif->hwaddr, 0U, tx_fn);
    uart_printf("[ptp] listening for a master (Announce)... start ptp4l on the LAN\n");
    s_started = true;
    return true;
}

void ptp_raw_poll(uint32_t now_ms)
{
    if (s_started) { ptp_core_poll(now_ms); }
}

bool ptp_raw_running(void)
{
    return s_started;
}
