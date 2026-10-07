/*
 * FlintRTOS - host simulation of the PTP slave engine (port/ptp/ptp_core.c).
 *
 * A simulated ptp4l-style master (two-step, E2E, 1 Sync/s, Announce/2 s) talks
 * to the real slave engine + servo + clock over a fake wire with a fixed path
 * delay and software-timestamp jitter. The slave's counter runs at 54 MHz with
 * a configurable oscillator error. Prints the slave's own log, then checks the
 * TRUE offset (slave clock vs master clock) at the end.
 *
 *   cc -std=c11 -DPTP_HOST_TEST -Ithird_party/ptpd/src -Iport/ptp \
 *      tests/ptp_sim.c port/ptp/ptp_core.c port/ptp/ptp_servo.c \
 *      port/ptp/ptp_clock.c port/ptp/ptp_msg.c port/ptp/ptp_time.c -o ptp_sim
 *   ./ptp_sim [drift_ppb] [delay_ns] [jitter_ns]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "ptp_core.h"
#include "ptp_clock.h"

uint64_t g_sim_cnt  = 0U;
uint64_t g_sim_freq = 54000000U;

#define NS 1000000000LL

static int64_t  T_now;                 /* true time (ns since sim start)       */
static int64_t  DRIFT_PPB  = 37000;    /* slave oscillator error               */
static int64_t  DELAY_NS   = 48000;    /* one-way wire+stack delay             */
static int64_t  JITTER_NS  = 8000;     /* software timestamp jitter (uniform)  */
static const int64_t MASTER_EPOCH = 1790000000LL * NS;  /* TAI-ish at sim start */
static const int64_t BOOT_T = 3 * NS;  /* slave booted 3 s before sim start     */

static uint64_t cnt_at(int64_t t)      /* slave counter at true time t          */
{
    long double s = (long double)(t + BOOT_T) / 1e9L;
    return (uint64_t)(s * (long double)g_sim_freq * (1.0L + (long double)DRIFT_PPB / 1e9L));
}
static int64_t master_time(int64_t t) { return MASTER_EPOCH + t; }
static int64_t jit(void) { return (JITTER_NS > 0) ? (int64_t)(rand() % (int)JITTER_NS) : 0; }

/* ---- wire -------------------------------------------------------------- */
typedef struct { int64_t at; uint16_t port; uint16_t len; uint8_t b[64]; int64_t stamp_extra; } Pkt;
static Pkt q[64]; static int qn;
static void enqueue(int64_t at, uint16_t port, const uint8_t *b, uint16_t len, int64_t extra)
{
    q[qn].at = at; q[qn].port = port; q[qn].len = len; memcpy(q[qn].b, b, len);
    q[qn].stamp_extra = extra; qn++;
}

static const uint8_t M_ID[10] = { 0xb8,0x27,0xeb,0xff,0xfe,0x12,0x34,0x56, 0,1 };

static void put16(uint8_t *p, uint16_t v) { p[0] = v >> 8; p[1] = (uint8_t)v; }
static void put_ts(uint8_t *p, int64_t ns)
{
    uint64_t s = (uint64_t)(ns / NS); uint32_t n = (uint32_t)(ns % NS);
    for (int i = 5; i >= 0; i--) { p[i] = (uint8_t)s; s >>= 8; }
    p[6] = n >> 24; p[7] = n >> 16; p[8] = n >> 8; p[9] = (uint8_t)n;
}
static void hdr(uint8_t *b, uint8_t type, uint16_t len, uint16_t seq, uint8_t f0, int8_t logi)
{
    memset(b, 0, len);
    b[0] = type; b[1] = 2; put16(&b[2], len); b[4] = 0; b[6] = f0; b[7] = 0;
    memcpy(&b[20], M_ID, 10); put16(&b[30], seq); b[33] = (uint8_t)logi;
}

static uint16_t sync_seq;
static void master_send_sync(void)
{
    uint8_t b[44];
    int64_t t1 = master_time(T_now) + jit();          /* master's own SW TX stamp */
    hdr(b, 0x0, 44, sync_seq, 0x02, 0);               /* two-step */
    enqueue(T_now + DELAY_NS, 319, b, 44, jit());
    hdr(b, 0x8, 44, sync_seq, 0, 0); put_ts(&b[34], t1);
    enqueue(T_now + DELAY_NS + 200000, 320, b, 44, 0);
    sync_seq++;
}
static void master_send_announce(uint16_t seq)
{
    uint8_t b[64];
    hdr(b, 0xB, 64, seq, 0, 1);
    b[7] = 0x0C; put16(&b[44], 37);                   /* PTP timescale, UTC offset 37 */
    b[47] = 128; b[48] = 248; b[49] = 0xFE; put16(&b[50], 0xFFFF); b[52] = 128;
    memcpy(&b[53], M_ID, 8);
    enqueue(T_now + DELAY_NS, 320, b, 64, 0);
}

/* Slave TX callback: the "driver" stamps TX at the doorbell (= now). */
static bool sim_tx(uint16_t port, const uint8_t *buf, uint16_t len, uint64_t *tx_cnt)
{
    (void)port; (void)len;
    *tx_cnt = cnt_at(T_now);
    /* Master receives it after the path delay and answers with Delay_Resp. */
    {
        uint8_t r[54];
        int64_t t4 = master_time(T_now + DELAY_NS) + jit();
        hdr(r, 0x9, 54, (uint16_t)((buf[30] << 8) | buf[31]), 0, 0);
        put_ts(&r[34], t4);
        memcpy(&r[44], &buf[20], 10);                 /* requestingPortIdentity */
        enqueue(T_now + DELAY_NS + 300000 + DELAY_NS, 320, r, 54, 0);
    }
    return true;
}

int main(int argc, char **argv)
{
    static const uint8_t mac[6] = { 0x02, 0x00, 0x5E, 0x00, 0x00, 0x01 };
    int64_t end = 120 * NS, worst = 0, sum = 0; int n = 0;

    if (argc > 1) { DRIFT_PPB = atoll(argv[1]); }
    if (argc > 2) { DELAY_NS  = atoll(argv[2]); }
    if (argc > 3) { JITTER_NS = atoll(argv[3]); }
    srand(1);
    printf("sim: drift=%lld ppb delay=%lld ns jitter=%lld ns\n",
           (long long)DRIFT_PPB, (long long)DELAY_NS, (long long)JITTER_NS);

    ptp_core_init(mac, 0, sim_tx);

    for (T_now = 0; T_now < end; T_now += 1000000)    /* 1 ms steps */
    {
        if ((T_now % NS) == 0)                   { master_send_sync(); }
        if ((T_now % (2 * NS)) == 500000000LL)   { master_send_announce((uint16_t)(T_now / NS)); }

        for (int i = 0; i < qn; )
        {
            if (q[i].at <= T_now)
            {
                /* Driver RX stamp: arrival + ISR/stack jitter. */
                g_sim_cnt = cnt_at(q[i].at + q[i].stamp_extra);
                ptp_core_rx(q[i].b, q[i].len, q[i].port, g_sim_cnt);
                q[i] = q[--qn];
            }
            else { i++; }
        }

        g_sim_cnt = cnt_at(T_now);
        ptp_core_poll((uint32_t)(T_now / 1000000));

        if ((T_now >= 60 * NS) && ((T_now % (NS / 10)) == 0))
        {
            int64_t off = ptp_clock_from_cnt(cnt_at(T_now)) - master_time(T_now);
            int64_t a = (off < 0) ? -off : off;
            if (a > worst) { worst = a; }
            sum += off; n++;
        }
    }

    printf("\nTRUE offset over the last 60 s: mean=%lld ns  worst=%lld ns  (freq=%d ppb, expect ~%lld)\n",
           (long long)(sum / n), (long long)worst, (int)ptp_clock_get_freq(), (long long)-DRIFT_PPB);
    {
        int64_t lim = 4 * JITTER_NS + 5000;
        bool ok = ptp_core_is_locked() && (worst < lim);
        printf("%s (limit %lld ns)\n", ok ? "PASS" : "FAIL", (long long)lim);
        return ok ? 0 : 1;
    }
}
