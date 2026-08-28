/*
 * FlintRTOS - PTP ordinary-clock slave over lwIP sockets.
 *
 * Software-timestamped: t2 (Sync RX) and t3 (Delay_Req TX) are read from the
 * disciplined clock at the socket call. The GENET MAC's IEEE-1588 hardware
 * timestamps replace those reads for sub-microsecond accuracy once wired.
 */
#include "ptp_slave.h"
#include "ptp_config.h"
#include "ptp_msg.h"
#include "ptp_time.h"
#include "ptp_servo.h"
#include "ptp_clock.h"

#include "uart.h"
#include "task.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"

extern void *memset(void *, int, unsigned long);

typedef struct
{
    int       ev_sock;     /* event port 319   */
    int       gen_sock;    /* general port 320 */
    MsgHeader self;        /* our portIdentity  */
    PtpServo  servo;

    uint16_t  sync_seq;    /* last Sync sequenceId */
    int       have_t1, have_t2, have_t3, have_t4;
    int64_t   t1_ns, t2_ns, t3_ns, t4_ns;
    uint16_t  dreq_seq;    /* our Delay_Req sequenceId */
    uint16_t  dreq_counter;
} PtpSlave;

static PtpSlave g_ptp;

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
    addr.sin_family = AF_INET;
    addr.sin_port   = lwip_htons(port);
    addr.sin_addr.s_addr = 0;   /* INADDR_ANY */
    if (lwip_bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        lwip_close(s);
        return -1;
    }

    /* Join the PTP primary multicast group. */
    mreq.imr_multiaddr.s_addr = ipaddr_addr(PTP_MCAST_ADDR);
    mreq.imr_interface.s_addr = 0;
    (void)lwip_setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    tv.tv_sec = 0; tv.tv_usec = 10000;   /* 10 ms poll */
    (void)lwip_setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    return s;
}

static void compute_and_discipline(void)
{
    if (g_ptp.have_t1 && g_ptp.have_t2 && g_ptp.have_t3 && g_ptp.have_t4)
    {
        int64_t ms_to_s = g_ptp.t2_ns - g_ptp.t1_ns;   /* master->slave leg */
        int64_t s_to_ms = g_ptp.t4_ns - g_ptp.t3_ns;   /* slave->master leg */
        int64_t offset  = (ms_to_s - s_to_ms) / 2;
        int64_t delay   = (ms_to_s + s_to_ms) / 2;
        int64_t corr    = ptp_servo_run(&g_ptp.servo, offset);

        ptp_clock_step(corr);

        uart_printf("[ptp] offset=%d ns  path_delay=%d ns  corr=%d ns\n",
                    (int)offset, (int)delay, (int)corr);

        g_ptp.have_t1 = 0; g_ptp.have_t2 = 0;
        g_ptp.have_t3 = 0; g_ptp.have_t4 = 0;
    }
}

static void send_delay_req(void)
{
    uint8_t buf[DELAY_REQ_LENGTH];
    Timestamp origin;
    struct sockaddr_in dst;
    TimeInternal now;

    ptp_clock_now(&now);
    ptp_internal_to_ts(&now, &origin);

    g_ptp.dreq_seq++;
    (void)ptp_pack_delay_req(buf, &g_ptp.self, g_ptp.dreq_seq, &origin);

    /* t3 = our TX timestamp (software). */
    g_ptp.t3_ns = ptp_clock_now_ns();
    g_ptp.have_t3 = 1;

    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port   = lwip_htons(PTP_EVENT_PORT);
    dst.sin_addr.s_addr = ipaddr_addr(PTP_MCAST_ADDR);
    (void)lwip_sendto(g_ptp.ev_sock, buf, sizeof(buf), 0,
                      (struct sockaddr *)&dst, sizeof(dst));
}

static void handle_event(const uint8_t *buf, int len)
{
    MsgHeader hdr;
    uint8_t   type;
    if (len < (int)PTP_HEADER_LENGTH) { return; }

    type = ptp_unpack_header(buf, &hdr);
    if (type == (uint8_t)PTP_SYNC)
    {
        g_ptp.t2_ns   = ptp_clock_now_ns();   /* software RX timestamp */
        g_ptp.have_t2 = 1;
        g_ptp.sync_seq = hdr.sequenceId;

        if (!ptp_header_two_step(&hdr))
        {
            /* One-step: originTimestamp is in the Sync body. */
            Timestamp ts; TimeInternal ti;
            ptp_unpack_timestamp(&buf[PTP_HEADER_LENGTH], &ts);
            ptp_ts_to_internal(&ts, &ti);
            g_ptp.t1_ns = ptp_time_to_ns(&ti);
            g_ptp.have_t1 = 1;
        }
        /* Two-step: t1 arrives in Follow_Up (general port). */
    }
}

static void handle_general(const uint8_t *buf, int len)
{
    MsgHeader hdr;
    uint8_t   type;
    if (len < (int)PTP_HEADER_LENGTH) { return; }

    type = ptp_unpack_header(buf, &hdr);

    if (type == (uint8_t)PTP_FOLLOW_UP)
    {
        if (hdr.sequenceId == g_ptp.sync_seq)
        {
            Timestamp ts; TimeInternal ti;
            ptp_unpack_timestamp(&buf[PTP_HEADER_LENGTH], &ts);
            ptp_ts_to_internal(&ts, &ti);
            g_ptp.t1_ns = ptp_time_to_ns(&ti);
            g_ptp.have_t1 = 1;
        }
    }
    else if (type == (uint8_t)PTP_DELAY_RESP)
    {
        if (hdr.sequenceId == g_ptp.dreq_seq)
        {
            Timestamp ts; TimeInternal ti;
            ptp_unpack_timestamp(&buf[PTP_HEADER_LENGTH], &ts);  /* receiveTimestamp */
            ptp_ts_to_internal(&ts, &ti);
            g_ptp.t4_ns = ptp_time_to_ns(&ti);
            g_ptp.have_t4 = 1;
        }
    }
    else
    {
        /* Announce / other: simplified BMC accepts the current master. */
    }
}

void ptp_slave_run(const uint8_t mac[6])
{
    uint8_t buf[128];

    memset(&g_ptp, 0, sizeof(g_ptp));
    ptp_servo_init(&g_ptp.servo);

    /* Local clock identity from MAC (EUI-64 style: mac[0..2] FF FE mac[3..5]). */
    g_ptp.self.sourcePortIdentity.clockIdentity[0] = mac[0];
    g_ptp.self.sourcePortIdentity.clockIdentity[1] = mac[1];
    g_ptp.self.sourcePortIdentity.clockIdentity[2] = mac[2];
    g_ptp.self.sourcePortIdentity.clockIdentity[3] = 0xFF;
    g_ptp.self.sourcePortIdentity.clockIdentity[4] = 0xFE;
    g_ptp.self.sourcePortIdentity.clockIdentity[5] = mac[3];
    g_ptp.self.sourcePortIdentity.clockIdentity[6] = mac[4];
    g_ptp.self.sourcePortIdentity.clockIdentity[7] = mac[5];
    g_ptp.self.sourcePortIdentity.portNumber = 1U;
    g_ptp.self.domainNumber = 0U;

    g_ptp.ev_sock  = make_socket(PTP_EVENT_PORT);
    g_ptp.gen_sock = make_socket(PTP_GENERAL_PORT);
    if ((g_ptp.ev_sock < 0) || (g_ptp.gen_sock < 0))
    {
        uart_printf("[ptp] socket setup failed\n");
        return;
    }
    uart_printf("[ptp] slave up (event:319 general:320 mcast %s)\n", PTP_MCAST_ADDR);

    for (;;)
    {
        int n = lwip_recv(g_ptp.ev_sock, buf, sizeof(buf), 0);
        if (n > 0) { handle_event(buf, n); }

        n = lwip_recv(g_ptp.gen_sock, buf, sizeof(buf), 0);
        if (n > 0) { handle_general(buf, n); }

        compute_and_discipline();

        /* Issue a Delay_Req roughly once per second. */
        g_ptp.dreq_counter++;
        if (g_ptp.dreq_counter >= 100U)
        {
            g_ptp.dreq_counter = 0U;
            send_delay_req();
        }
        vTaskDelay(10U);
    }
}

void vPtpTask(void *pvParameters)
{
    static const uint8_t k_mac[6] = { 0x02U, 0x00U, 0x5EU, 0x00U, 0x00U, 0x01U };
    (void)pvParameters;
    ptp_slave_run(k_mac);
}
