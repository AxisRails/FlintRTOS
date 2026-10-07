/*
 * FlintRTOS - PTP ordinary-clock slave over lwIP sockets (OS mode).
 *
 * Thin transport around ptp_core. In OS mode frames reach lwIP through the
 * tcpip thread's mailbox, so the driver's RX stamp cannot be tied to a socket
 * read; this binding stamps at the socket call instead (coarser: expect tens
 * to hundreds of microseconds). The NO_SYS binding (ptp_lwip_raw.c) uses the
 * GENET interrupt-time stamps and is the accurate path.
 */
#include "ptp_slave.h"
#include "ptp_core.h"
#include "ptp_config.h"
#include "ptp_clock.h"

#include "uart.h"
#include "task.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"

extern void *memset(void *, int, unsigned long);

static int s_ev = -1;
static int s_gen = -1;

static int make_socket(uint16_t port)
{
    int s = lwip_socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    struct ip_mreq mreq;
    struct timeval tv;
    int one = 1;

    if (s < 0) { return -1; }
    (void)lwip_setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = lwip_htons(port);
    addr.sin_addr.s_addr = 0;   /* INADDR_ANY */
    if (lwip_bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        lwip_close(s);
        return -1;
    }

    mreq.imr_multiaddr.s_addr = ipaddr_addr(PTP_MCAST_ADDR);
    mreq.imr_interface.s_addr = 0;
    (void)lwip_setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    tv.tv_sec = 0; tv.tv_usec = 2000;   /* 2 ms poll */
    (void)lwip_setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    return s;
}

static bool tx_fn(uint16_t port, const uint8_t *buf, uint16_t len, uint64_t *tx_cnt)
{
    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family      = AF_INET;
    dst.sin_port        = lwip_htons(port);
    dst.sin_addr.s_addr = ipaddr_addr(PTP_MCAST_ADDR);
    *tx_cnt = ptp_clock_read_cnt();          /* socket-level TX stamp */
    return lwip_sendto((port == (uint16_t)PTP_EVENT_PORT) ? s_ev : s_gen, buf, len, 0,
                       (struct sockaddr *)&dst, sizeof(dst)) >= 0;
}

void ptp_slave_run(const uint8_t mac[6])
{
    uint8_t buf[128];

    s_ev  = make_socket((uint16_t)PTP_EVENT_PORT);
    s_gen = make_socket((uint16_t)PTP_GENERAL_PORT);
    if ((s_ev < 0) || (s_gen < 0))
    {
        uart_printf("[ptp] socket setup failed\n");
        return;
    }
    ptp_core_init(mac, 0U, tx_fn);

    for (;;)
    {
        int n = lwip_recv(s_ev, buf, sizeof(buf), 0);
        if (n > 0) { ptp_core_rx(buf, (uint16_t)n, (uint16_t)PTP_EVENT_PORT, ptp_clock_read_cnt()); }

        n = lwip_recv(s_gen, buf, sizeof(buf), 0);
        if (n > 0) { ptp_core_rx(buf, (uint16_t)n, (uint16_t)PTP_GENERAL_PORT, ptp_clock_read_cnt()); }

        ptp_core_poll((uint32_t)xTaskGetTickCount());
    }
}

void vPtpTask(void *pvParameters)
{
    static const uint8_t k_mac[6] = { 0x02U, 0x00U, 0x5EU, 0x00U, 0x00U, 0x01U };
    (void)pvParameters;
    ptp_slave_run(k_mac);
}
