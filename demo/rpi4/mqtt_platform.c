/*
 * FlintRTOS - MQTT platform services on the Raspberry Pi 4 (mqtt_platform.h).
 */
#include "FlintRTOS.h"

#if (configUSE_LWIP == 1) && (configUSE_CORE_MQTT == 1)

#include "task.h"
#include "uart.h"
#include "rpi4.h"
#include "portable.h"
#include "portmacro.h"

#include "mqtt_platform.h"
#include "jsonw.h"
#include "net_demo.h"

#include "lwip/netif.h"
#include "lwip/ip4_addr.h"

#if (configUSE_PTP == 1)
#include "ptp_core.h"
#include "ptp_lwip_raw.h"
#endif

/* ACT (green) LED: GPIO42 on the Pi 4, active high. */
#define LED_GPIO        (42U)
#define GPFSEL4         (GPIO_BASE + 0x10UL)
#define GPSET1          (GPIO_BASE + 0x20UL)
#define GPCLR1          (GPIO_BASE + 0x2CUL)

static int  s_led = -1;   /* -1 = not configured yet */

uint32_t fm_plat_ms(void)
{
    return (uint32_t)xTaskGetTickCount();          /* 1 kHz tick */
}

void fm_plat_pump(bool idle)
{
    net_pump(idle);
}

const char *fm_plat_ip(void)
{
    return ip4addr_ntoa(netif_ip4_addr(net_demo_netif()));
}

void fm_plat_log(const char *line)
{
    uart_printf("%s\n", line);
}

int fm_plat_led(int on)
{
    if (s_led < 0)
    {
        uint32_t v = mmio_read32(GPFSEL4);
        v &= ~(7U << ((LED_GPIO - 40U) * 3U));
        v |=  (1U << ((LED_GPIO - 40U) * 3U));     /* output */
        mmio_write32(GPFSEL4, v);
        s_led = 0;
    }
    s_led = (on < 0) ? ((s_led != 0) ? 0 : 1) : ((on != 0) ? 1 : 0);
    mmio_write32((s_led != 0) ? GPSET1 : GPCLR1, 1U << (LED_GPIO - 32U));
    return s_led;
}

size_t fm_plat_ptp_json(char *buf, size_t cap)
{
#if (configUSE_PTP == 1)
    JsonW     w;
    PtpStatus st;
    char      when[28];
    char      gm[17];

    if (!ptp_raw_running()) { return 0U; }
    ptp_core_get_status(&st);

    jw_init(&w, buf, cap);
    jw_obj_open(&w);
    jw_str(&w, "state", st.state);
    jw_bool(&w, "locked", st.locked);
    if (st.have_master)
    {
        uint64_t id = 0U;
        for (uint32_t i = 0U; i < 8U; i++) { id = (id << 8) | st.gm_id[i]; }
        jw_str(&w, "gm", fm_hex64(id, gm, 16U));
        jw_i64(&w, "offset_ns", st.offset_ns);
        jw_i64(&w, "delay_ns", st.delay_ns);
        jw_i64(&w, "freq_ppb", st.freq_ppb);
        jw_u32(&w, "syncs", st.n_sync);
    }
    if (st.locked)
    {
        ptp_format_utc(ptp_core_utc_now_ns(), when);
        jw_str(&w, "utc", when);
    }
    jw_obj_close(&w);
    return jw_ok(&w) ? jw_len(&w) : 0U;
#else
    (void)buf; (void)cap;
    return 0U;
#endif
}

size_t fm_plat_stats_json(char *buf, size_t cap)
{
    JsonW          w;
    TaskSnapshot_t ts[12];
    UBaseType_t    n = uxTaskGetSnapshot(ts, 12U);

    jw_init(&w, buf, cap);
    jw_obj_open(&w);
    jw_u32(&w, "uptime_s", fm_plat_ms() / 1000U);
    jw_u32(&w, "heap_free", (uint32_t)xPortGetFreeHeapSize());
    jw_u32(&w, "heap_min",  (uint32_t)xPortGetMinimumEverFreeHeapSize());
    jw_u32(&w, "genet_irqs", ulPortIrqCount(GENET_IRQ0_INTID));
    jw_arr_open(&w, "tasks");
    for (UBaseType_t i = 0U; i < n; i++)
    {
        jw_obj_open(&w);
        jw_str(&w, "name", ts[i].pcName);
        jw_u32(&w, "prio", (uint32_t)ts[i].uxPriority);
        jw_u32(&w, "stack", ts[i].ulStackDepth * (uint32_t)sizeof(StackType_t));
        jw_u32(&w, "stack_free", ts[i].ulStackFree * (uint32_t)sizeof(StackType_t));
        jw_obj_close(&w);
    }
    jw_arr_close(&w);
    jw_obj_close(&w);
    return jw_ok(&w) ? jw_len(&w) : 0U;
}

#endif
