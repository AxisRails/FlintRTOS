/*
 * FlintRTOS - IEEE 1588 ordinary-clock slave engine. See ptp_core.h.
 *
 * Measurement model (E2E):
 *   Sync/Follow_Up : t1 (master TX, + correctionField)  t2 (our RX stamp)
 *   Delay_Req/Resp : t3 (our TX stamp)   t4 (master RX, - correctionField)
 *   master->slave leg  ms = t2 - t1 = delay + offset
 *   slave->master leg  sm = t4 - t3 = delay - offset
 *   meanPathDelay = (ms + sm) / 2  -> median of the last PTP_DELAY_WIN samples
 *   offsetFromMaster = ms - meanPathDelay   -> PI servo (ptp_servo.c)
 *
 * All times are int64 ns on the PTP timescale; timestamps arrive as counter
 * ticks and are converted with ptp_clock_from_cnt() immediately. A clock step
 * bumps ptp_clock_epoch(); any exchange whose halves straddle a step is
 * dropped, since its timestamps come from two different timescales.
 */
#include "ptp_core.h"
#include "ptp_config.h"
#include "ptp_msg.h"
#include "ptp_time.h"
#include "ptp_clock.h"
#include "ptp_servo.h"

#include <stddef.h>

#ifdef PTP_HOST_TEST
#include <stdio.h>
#include <string.h>
#define ptp_log printf
#else
#include "uart.h"
#define ptp_log uart_printf
extern void *memset(void *dest, int c, size_t n);
#endif

#define NS_PER_S          (1000000000LL)
#define PTP_DELAY_WIN     (8U)
#define PTP_DELAY_MAX_NS  (10000000LL)     /* reject |delay| > 10 ms       */
#define PTP_STATS_WIN     (16U)
#define ANNOUNCE_LEN_MIN  (64U)
#define DELAY_RESP_LEN    (54U)
#define SYNC_LEN          (44U)
#define FLAG1_UTC_VALID   (0x04U)
#define FLAG1_PTP_TIMESCALE (0x08U)

/* ---- Small helpers (freestanding: no libc beyond memset/memcpy) ---------- */
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(((uint16_t)p[0] << 8) | p[1]); }
static uint32_t rd32(const uint8_t *p)
{ return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }

/* 10-byte PTP Timestamp (48-bit seconds + 32-bit ns) -> ns. */
static int64_t rd_ts_ns(const uint8_t *p)
{
    uint64_t secs = 0U;
    for (uint32_t i = 0U; i < 6U; i++) { secs = (secs << 8) | p[i]; }
    return ((int64_t)secs * NS_PER_S) + (int64_t)rd32(&p[6]);
}

/* correctionField is ns * 2^16 (signed); keep the integer ns part. */
static int64_t rd_corr_ns(const uint8_t *hdr)
{
    uint64_t v = 0U;
    for (uint32_t i = 0U; i < 8U; i++) { v = (v << 8) | hdr[8U + i]; }
    return ((int64_t)v) / 65536;
}

static int cmp_bytes(const uint8_t *a, const uint8_t *b, uint32_t n)
{
    for (uint32_t i = 0U; i < n; i++)
    {
        if (a[i] != b[i]) { return (a[i] < b[i]) ? -1 : 1; }
    }
    return 0;
}

static void copy_bytes(uint8_t *d, const uint8_t *s, uint32_t n)
{
    for (uint32_t i = 0U; i < n; i++) { d[i] = s[i]; }
}

static int64_t abs64(int64_t v) { return (v < 0) ? -v : v; }

/* int64 -> decimal string (uart_printf has no %lld). */
static const char *i64s(int64_t v, char *buf)
{
    char tmp[24];
    uint32_t n = 0U, o = 0U;
    uint64_t u = (v < 0) ? (uint64_t)(-(v + 1)) + 1U : (uint64_t)v;
    do { tmp[n++] = (char)('0' + (u % 10U)); u /= 10U; } while ((u != 0U) && (n < sizeof(tmp)));
    if (v < 0) { buf[o++] = '-'; }
    while (n > 0U) { buf[o++] = tmp[--n]; }
    buf[o] = '\0';
    return buf;
}

static uint64_t isqrt64(uint64_t x)
{
    uint64_t r = 0U, bit = 1ULL << 62;
    while (bit > x) { bit >>= 2; }
    while (bit != 0U)
    {
        if (x >= r + bit) { x -= r + bit; r = (r >> 1) + bit; }
        else              { r >>= 1; }
        bit >>= 2;
    }
    return r;
}

/* ---- State ---------------------------------------------------------------- */
typedef struct
{
    uint8_t  prio1, clk_class, clk_accuracy, prio2;
    uint16_t variance, steps_removed;
    uint8_t  gm_id[8];
    uint8_t  port_id[10];          /* sender clockIdentity + portNumber */
} Dataset;

typedef struct
{
    PtpTxFn   tx;
    MsgHeader self;                /* domain + our sourcePortIdentity   */
    uint8_t   self_id[10];

    /* selected master */
    bool      have_master;
    Dataset   master;
    uint32_t  last_announce_ms;
    uint32_t  announce_timeout_ms;
    int16_t   utc_offset;
    uint8_t   flags1;

    /* Sync / Follow_Up */
    bool      sync_waiting_fup;
    uint16_t  sync_seq;
    uint64_t  t2_cnt;
    int64_t   t2_ns;
    int64_t   sync_corr;
    uint32_t  t2_epoch;

    bool      ms_valid;            /* last master->slave leg usable?     */
    int64_t   ms_leg;
    uint32_t  ms_epoch;

    /* Delay_Req / Delay_Resp */
    bool      dreq_due;
    bool      dreq_outstanding;
    uint16_t  dreq_seq;
    int64_t   t3_ns;
    uint32_t  t3_epoch;
    uint32_t  last_dreq_ms;
    uint32_t  now_ms;

    int64_t   delay_win[PTP_DELAY_WIN];
    uint32_t  delay_n, delay_idx;
    int64_t   delay_ns;            /* filtered; -1 = unknown             */

    PtpServo  servo;
    int64_t   last_offset;
    uint32_t  n_sync;

    /* stats over the last PTP_STATS_WIN locked samples */
    int64_t   st_sum, st_max;
    uint64_t  st_sq;
    uint32_t  st_n;
} PtpCore;

static PtpCore g;

/* ---- Master selection (IEEE 1588 BMC dataset comparison) ------------------ */
static int dataset_cmp(const Dataset *a, const Dataset *b)   /* <0: a better */
{
    int c;
    if (a->prio1 != b->prio1)               { return (a->prio1 < b->prio1) ? -1 : 1; }
    if (a->clk_class != b->clk_class)       { return (a->clk_class < b->clk_class) ? -1 : 1; }
    if (a->clk_accuracy != b->clk_accuracy) { return (a->clk_accuracy < b->clk_accuracy) ? -1 : 1; }
    if (a->variance != b->variance)         { return (a->variance < b->variance) ? -1 : 1; }
    if (a->prio2 != b->prio2)               { return (a->prio2 < b->prio2) ? -1 : 1; }
    c = cmp_bytes(a->gm_id, b->gm_id, 8U);
    if (c != 0)                             { return c; }
    if (a->steps_removed != b->steps_removed) { return (a->steps_removed < b->steps_removed) ? -1 : 1; }
    return cmp_bytes(a->port_id, b->port_id, 10U);
}

static void print_id(const char *label, const uint8_t *id)
{
    ptp_log("%s%x%x%x%x%x%x%x%x", label, id[0] >> 4, id[0] & 15U, id[1] >> 4, id[1] & 15U,
            id[2] >> 4, id[2] & 15U, id[3] >> 4, id[3] & 15U);
    ptp_log(".%x%x%x%x%x%x%x%x", id[4] >> 4, id[4] & 15U, id[5] >> 4, id[5] & 15U,
            id[6] >> 4, id[6] & 15U, id[7] >> 4, id[7] & 15U);
}

static void reset_measurements(void)
{
    g.sync_waiting_fup = false;
    g.ms_valid         = false;
    g.dreq_due         = false;
    g.dreq_outstanding = false;
    g.delay_n = 0U; g.delay_idx = 0U; g.delay_ns = -1;
    g.st_n = 0U; g.st_sum = 0; g.st_sq = 0U; g.st_max = 0;
    ptp_servo_init(&g.servo);
}

static uint32_t log_interval_ms(int8_t log2s)
{
    if (log2s < -7) { log2s = -7; }
    if (log2s >  6) { log2s =  6; }
    return (log2s >= 0) ? (1000U << (uint32_t)log2s) : (1000U >> (uint32_t)(-log2s));
}

static void handle_announce(const uint8_t *b, uint16_t len)
{
    Dataset ds;
    if (len < ANNOUNCE_LEN_MIN) { return; }

    ds.prio1         = b[47];
    ds.clk_class     = b[48];
    ds.clk_accuracy  = b[49];
    ds.variance      = rd16(&b[50]);
    ds.prio2         = b[52];
    copy_bytes(ds.gm_id, &b[53], 8U);
    ds.steps_removed = rd16(&b[61]);
    copy_bytes(ds.port_id, &b[20], 10U);

    if (g.have_master && (cmp_bytes(ds.port_id, g.master.port_id, 10U) == 0))
    {
        g.master = ds;                                 /* refresh */
    }
    else if (!g.have_master || (dataset_cmp(&ds, &g.master) < 0))
    {
        bool had = g.have_master;
        g.master      = ds;
        g.have_master = true;
        reset_measurements();
        ptp_log("\n[ptp] %s master: ", had ? "better" : "selected");
        print_id("port ", ds.port_id);
        ptp_log("/%u  gm ", (unsigned int)rd16(&ds.port_id[8]));
        print_id("", ds.gm_id);
        ptp_log("  prio1=%u class=%u acc=0x%x prio2=%u\n", (unsigned int)ds.prio1,
                (unsigned int)ds.clk_class, (unsigned int)ds.clk_accuracy, (unsigned int)ds.prio2);
    }
    else
    {
        return;                                        /* worse foreign master */
    }

    g.last_announce_ms    = g.now_ms;
    g.announce_timeout_ms = 3U * log_interval_ms((int8_t)b[33]) + 500U; /* receiptTimeout=3 */
    g.utc_offset          = (int16_t)rd16(&b[44]);
    g.flags1              = b[7];
}

static bool from_master(const uint8_t *b)
{
    return g.have_master && (cmp_bytes(&b[20], g.master.port_id, 10U) == 0);
}

/* ---- Reporting ------------------------------------------------------------ */
int64_t ptp_core_utc_now_ns(void)
{
    int64_t t = ptp_clock_now_ns();
    if (((g.flags1 & FLAG1_PTP_TIMESCALE) != 0U) && ((g.flags1 & FLAG1_UTC_VALID) != 0U))
    {
        t -= (int64_t)g.utc_offset * NS_PER_S;         /* TAI -> UTC */
    }
    return t;
}

static void put_num(char *p, uint32_t v, uint32_t width)
{
    for (uint32_t i = width; i > 0U; i--) { p[i - 1U] = (char)('0' + (v % 10U)); v /= 10U; }
}

void ptp_format_utc(int64_t utc_ns, char *buf)
{
    int64_t secs = utc_ns / NS_PER_S;
    int64_t ns   = utc_ns % NS_PER_S;
    if (ns < 0) { ns += NS_PER_S; secs -= 1; }
    {
        int64_t z   = (secs >= 0) ? (secs / 86400) : ((secs - 86399) / 86400);
        int64_t sod = secs - (z * 86400);
        /* civil_from_days (H. Hinnant), proleptic Gregorian. */
        int64_t zz  = z + 719468;
        int64_t era = ((zz >= 0) ? zz : (zz - 146096)) / 146097;
        int64_t doe = zz - (era * 146097);
        int64_t yoe = (doe - (doe / 1460) + (doe / 36524) - (doe / 146096)) / 365;
        int64_t y   = yoe + (era * 400);
        int64_t doy = doe - ((365 * yoe) + (yoe / 4) - (yoe / 100));
        int64_t mp  = ((5 * doy) + 2) / 153;
        int64_t d   = doy - (((153 * mp) + 2) / 5) + 1;
        int64_t m   = (mp < 10) ? (mp + 3) : (mp - 9);
        if (m <= 2) { y += 1; }

        put_num(&buf[0], (uint32_t)y, 4U);  buf[4] = '-';
        put_num(&buf[5], (uint32_t)m, 2U);  buf[7] = '-';
        put_num(&buf[8], (uint32_t)d, 2U);  buf[10] = ' ';
        put_num(&buf[11], (uint32_t)(sod / 3600), 2U);        buf[13] = ':';
        put_num(&buf[14], (uint32_t)((sod / 60) % 60), 2U);   buf[16] = ':';
        put_num(&buf[17], (uint32_t)(sod % 60), 2U);          buf[19] = '.';
        put_num(&buf[20], (uint32_t)(ns / 1000), 6U);         buf[26] = '\0';
    }
}

static void report(int64_t offset, PtpServoState before)
{
    char a[24], b[24], c[24], when[28];
    PtpServoState st = g.servo.state;

    if (st == PTP_SERVO_LOCKED)
    {
        int64_t ao = abs64(offset);
        g.st_sum += offset;
        g.st_sq  += (uint64_t)(ao * ao);
        if (ao > g.st_max) { g.st_max = ao; }
        g.st_n++;
    }

    ptp_format_utc(ptp_core_utc_now_ns(), when);
    if (before != PTP_SERVO_LOCKED)
    {
        /* Offset may be seconds before the first step: print it in full. */
        ptp_log("[ptp] %s -> %s  offset=%s ns  delay=%s ns  freq=%s ppb | %s UTC\n",
                ptp_servo_state_name(before), ptp_servo_state_name(st),
                i64s(offset, a), i64s(g.delay_ns, b), i64s(ptp_clock_get_freq(), c), when);
    }
    else
    {
        ptp_log("[ptp] #%u offset=%s ns  delay=%s ns  freq=%s ppb | %s UTC\n",
                (unsigned int)g.n_sync, i64s(offset, a), i64s(g.delay_ns, b),
                i64s(ptp_clock_get_freq(), c), when);
    }

    if (g.st_n >= PTP_STATS_WIN)
    {
        int64_t mean = g.st_sum / (int64_t)g.st_n;
        uint64_t rms = isqrt64(g.st_sq / g.st_n);
        ptp_log("[ptp] ---- last %u: mean=%s ns  rms=%s ns  max|offset|=%s ns ----\n",
                (unsigned int)g.st_n, i64s(mean, a), i64s((int64_t)rms, b), i64s(g.st_max, c));
        g.st_n = 0U; g.st_sum = 0; g.st_sq = 0U; g.st_max = 0;
    }
}

/* ---- Measurements ---------------------------------------------------------- */
static void delay_add(int64_t sample)
{
    int64_t s[PTP_DELAY_WIN];
    uint32_t n;

    g.delay_win[g.delay_idx] = sample;
    g.delay_idx = (g.delay_idx + 1U) % PTP_DELAY_WIN;
    if (g.delay_n < PTP_DELAY_WIN) { g.delay_n++; }

    n = g.delay_n;
    for (uint32_t i = 0U; i < n; i++) { s[i] = g.delay_win[i]; }
    for (uint32_t i = 1U; i < n; i++)                  /* insertion sort */
    {
        int64_t v = s[i];
        uint32_t j = i;
        while ((j > 0U) && (s[j - 1U] > v)) { s[j] = s[j - 1U]; j--; }
        s[j] = v;
    }
    g.delay_ns = ((n % 2U) == 1U) ? s[n / 2U] : ((s[(n / 2U) - 1U] + s[n / 2U]) / 2);
}

/* A Sync (+Follow_Up) pair is complete: run the servo. */
static void sync_complete(int64_t t1_ns, int64_t corr_ns)
{
    PtpServoState before = g.servo.state;
    int64_t raw, offset;

    if (g.t2_epoch != ptp_clock_epoch()) { return; }   /* straddled a step */

    raw = g.t2_ns - (t1_ns + corr_ns);
    g.n_sync++;

    offset = ptp_servo_sample(&g.servo, raw, g.delay_ns, g.t2_cnt);
    g.last_offset = offset;

    /* The ms leg is reusable for delay only if the servo did not just step. */
    if (ptp_clock_epoch() == g.t2_epoch)
    {
        g.ms_leg   = raw;
        g.ms_epoch = g.t2_epoch;
        g.ms_valid = true;
    }
    else
    {
        g.ms_valid = false;
    }

    report(offset, before);
    g.dreq_due = true;
}

static void handle_sync(const uint8_t *b, uint16_t len, uint64_t rx_cnt)
{
    if (len < SYNC_LEN) { return; }
    g.sync_seq  = rd16(&b[30]);
    g.t2_cnt    = rx_cnt;
    g.t2_ns     = ptp_clock_from_cnt(rx_cnt);
    g.t2_epoch  = ptp_clock_epoch();
    g.sync_corr = rd_corr_ns(b);

    if ((b[6] & 0x02U) != 0U)                  /* twoStepFlag */
    {
        g.sync_waiting_fup = true;             /* t1 comes in Follow_Up */
    }
    else
    {
        g.sync_waiting_fup = false;
        sync_complete(rd_ts_ns(&b[34]), g.sync_corr);
    }
}

static void handle_follow_up(const uint8_t *b, uint16_t len)
{
    if ((len < SYNC_LEN) || !g.sync_waiting_fup || (rd16(&b[30]) != g.sync_seq)) { return; }
    g.sync_waiting_fup = false;
    sync_complete(rd_ts_ns(&b[34]), g.sync_corr + rd_corr_ns(b));
}

static void handle_delay_resp(const uint8_t *b, uint16_t len)
{
    int64_t t4, sm, sample;

    if ((len < DELAY_RESP_LEN) || !g.dreq_outstanding) { return; }
    if ((rd16(&b[30]) != g.dreq_seq) || (cmp_bytes(&b[44], g.self_id, 10U) != 0)) { return; }
    g.dreq_outstanding = false;

    if ((g.t3_epoch != ptp_clock_epoch()) || !g.ms_valid || (g.ms_epoch != g.t3_epoch))
    {
        return;                                /* halves from different timescales */
    }

    t4     = rd_ts_ns(&b[34]) - rd_corr_ns(b);
    sm     = t4 - g.t3_ns;
    sample = (g.ms_leg + sm) / 2;
    if (abs64(sample) > PTP_DELAY_MAX_NS) { return; }
    delay_add(sample);
}

static void send_delay_req(void)
{
    uint8_t   buf[DELAY_REQ_LENGTH];
    Timestamp origin;
    TimeInternal now;
    uint64_t  tx_cnt = 0U;

    ptp_clock_now(&now);
    ptp_internal_to_ts(&now, &origin);
    g.dreq_seq++;
    (void)ptp_pack_delay_req(buf, &g.self, g.dreq_seq, &origin);

    if ((g.tx != NULL) && g.tx((uint16_t)PTP_EVENT_PORT, buf, (uint16_t)sizeof(buf), &tx_cnt))
    {
        g.t3_ns            = ptp_clock_from_cnt(tx_cnt);
        g.t3_epoch         = ptp_clock_epoch();
        g.dreq_outstanding = true;
    }
    g.last_dreq_ms = g.now_ms;
}

/* ---- Public API ------------------------------------------------------------ */
void ptp_core_init(const uint8_t mac[6], uint8_t domain, PtpTxFn tx)
{
    uint8_t *id = (uint8_t *)g.self.sourcePortIdentity.clockIdentity;

    (void)memset(&g, 0, sizeof(g));
    g.tx = tx;

    /* EUI-64 clockIdentity from the MAC: mac[0..2] FF FE mac[3..5]. */
    id[0] = mac[0]; id[1] = mac[1]; id[2] = mac[2];
    id[3] = 0xFFU;  id[4] = 0xFEU;
    id[5] = mac[3]; id[6] = mac[4]; id[7] = mac[5];
    g.self.sourcePortIdentity.portNumber = 1U;
    g.self.domainNumber = domain;
    copy_bytes(g.self_id, id, 8U);
    g.self_id[8] = 0U; g.self_id[9] = 1U;

    reset_measurements();
    print_id("[ptp] slave clockIdentity ", id);
    ptp_log("  domain %u, E2E, UDPv4 multicast %s\n", (unsigned int)domain, PTP_MCAST_ADDR);
}

void ptp_core_rx(const uint8_t *buf, uint16_t len, uint16_t port, uint64_t rx_cnt)
{
    uint8_t type;

    if (len < PTP_HEADER_LENGTH)                       { return; }
    if ((buf[1] & 0x0FU) != 2U)                        { return; }   /* PTPv2 only */
    if (buf[4] != g.self.domainNumber)                 { return; }
    if (cmp_bytes(&buf[20], g.self_id, 10U) == 0)      { return; }   /* our own */

    type = (uint8_t)(buf[0] & 0x0FU);
    if (port == (uint16_t)PTP_EVENT_PORT)
    {
        if ((type == (uint8_t)PTP_SYNC) && from_master(buf)) { handle_sync(buf, len, rx_cnt); }
    }
    else
    {
        if (type == (uint8_t)PTP_ANNOUNCE)                               { handle_announce(buf, len); }
        else if ((type == (uint8_t)PTP_FOLLOW_UP) && from_master(buf))   { handle_follow_up(buf, len); }
        else if ((type == (uint8_t)PTP_DELAY_RESP) && from_master(buf))  { handle_delay_resp(buf, len); }
        else { /* management / signaling / peer delay: not used in E2E slave */ }
    }
}

void ptp_core_poll(uint32_t now_ms)
{
    g.now_ms = now_ms;

    if (g.have_master && ((uint32_t)(now_ms - g.last_announce_ms) > g.announce_timeout_ms))
    {
        ptp_log("[ptp] master announce timeout - back to LISTENING (clock free-runs at %d ppb)\n",
                (int)ptp_clock_get_freq());
        g.have_master = false;
        reset_measurements();
        return;
    }

    /* One Delay_Req per completed Sync, at most ~1/s (logMinDelayReqInterval 0),
       and only once the clock is on the master's timescale. */
    if (g.dreq_due && (g.servo.state != PTP_SERVO_UNLOCKED) &&
        ((uint32_t)(now_ms - g.last_dreq_ms) >= 900U))
    {
        g.dreq_due = false;
        send_delay_req();
    }
}

bool ptp_core_is_locked(void)
{
    return g.servo.state == PTP_SERVO_LOCKED;
}

int64_t ptp_core_last_offset_ns(void)
{
    return g.last_offset;
}

void ptp_core_get_status(PtpStatus *st)
{
    st->have_master = g.have_master;
    st->locked      = g.have_master && (g.servo.state == PTP_SERVO_LOCKED);
    st->state       = g.have_master ? ptp_servo_state_name(g.servo.state) : "LISTENING";
    st->offset_ns   = g.last_offset;
    st->delay_ns    = g.delay_ns;
    st->freq_ppb    = ptp_clock_get_freq();
    st->n_sync      = g.n_sync;
    copy_bytes(st->gm_id, g.master.gm_id, 8U);
    st->utc_offset  = g.utc_offset;
}
