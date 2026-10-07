/*
 * FlintRTOS - VideoCore property mailbox (BCM2711). See mailbox.h.
 *
 * Protocol: write (bus_addr(buffer) | channel) to MBOX1 WRITE once its FULL
 * flag clears, then poll MBOX0 until a message for our channel arrives. The
 * buffer must be 16-byte aligned. RAM is mapped Normal Non-Cacheable, so the
 * GPU sees our writes without cache maintenance. The GPU addresses ARM RAM
 * through the 0xC0000000 (uncached) bus alias; some firmware also accepts the
 * plain physical address, so both are tried. Every spin is bounded.
 */
#include "mailbox.h"
#include "rpi4.h"

#define MBOX_BASE        (0xFE00B880UL)
#define MBOX_READ        (MBOX_BASE + 0x00UL)
#define MBOX_STATUS      (MBOX_BASE + 0x18UL)
#define MBOX_WRITE       (MBOX_BASE + 0x20UL)
#define MBOX_FULL        (0x80000000U)
#define MBOX_EMPTY       (0x40000000U)
#define MBOX_CH_PROP     (8U)
#define MBOX_REQUEST     (0x00000000U)
#define MBOX_RESPONSE_OK (0x80000000U)
#define MBOX_SPIN        (2000000U)

#define TAG_GET_MAC      (0x00010003U)
#define TAG_GET_SERIAL   (0x00010004U)
#define TAG_END          (0x00000000U)

static volatile uint32_t s_mbox[16] __attribute__((aligned(16)));

static bool mbox_call_at(uint32_t bus_addr)
{
    uint32_t msg = (bus_addr & ~0xFU) | MBOX_CH_PROP;
    uint32_t i;

    __asm__ volatile("dsb sy" ::: "memory");
    for (i = 0U; (i < MBOX_SPIN) && ((mmio_read32(MBOX_STATUS) & MBOX_FULL) != 0U); i++) { }
    if (i == MBOX_SPIN) { return false; }
    mmio_write32(MBOX_WRITE, msg);

    for (i = 0U; i < MBOX_SPIN; i++)
    {
        if ((mmio_read32(MBOX_STATUS) & MBOX_EMPTY) == 0U)
        {
            if (mmio_read32(MBOX_READ) == msg)
            {
                __asm__ volatile("dsb sy" ::: "memory");
                return (s_mbox[1] == MBOX_RESPONSE_OK);
            }
        }
    }
    return false;
}

/* One-tag property request; value buffer = vwords words. */
static bool mbox_query(uint32_t tag, uint32_t vwords)
{
    uint32_t base = (uint32_t)(uintptr_t)&s_mbox[0];
    uint32_t k;

    for (uint32_t pass = 0U; pass < 2U; pass++)
    {
        s_mbox[0] = (5U + vwords + 1U) * 4U;   /* total size in bytes   */
        s_mbox[1] = MBOX_REQUEST;
        s_mbox[2] = tag;
        s_mbox[3] = vwords * 4U;               /* value buffer size     */
        s_mbox[4] = 0U;                        /* request code          */
        for (k = 0U; k < vwords; k++) { s_mbox[5U + k] = 0U; }
        s_mbox[5U + vwords] = TAG_END;

        if (mbox_call_at((pass == 0U) ? (base | 0xC0000000U) : base) &&
            ((s_mbox[4] & 0x80000000U) != 0U))
        {
            return true;
        }
    }
    return false;
}

bool mbox_get_mac(uint8_t mac[6])
{
    uint8_t m[6];
    if (!mbox_query(TAG_GET_MAC, 2U)) { return false; }
    m[0] = (uint8_t)(s_mbox[5]);        m[1] = (uint8_t)(s_mbox[5] >> 8);
    m[2] = (uint8_t)(s_mbox[5] >> 16);  m[3] = (uint8_t)(s_mbox[5] >> 24);
    m[4] = (uint8_t)(s_mbox[6]);        m[5] = (uint8_t)(s_mbox[6] >> 8);
    /* Reject all-zero / all-ones / multicast answers. */
    if (((m[0] | m[1] | m[2] | m[3] | m[4] | m[5]) == 0U) ||
        ((m[0] & m[1] & m[2] & m[3] & m[4] & m[5]) == 0xFFU) ||
        ((m[0] & 0x01U) != 0U))
    {
        return false;
    }
    for (uint32_t i = 0U; i < 6U; i++) { mac[i] = m[i]; }
    return true;
}

bool mbox_get_serial(uint64_t *serial)
{
    if (!mbox_query(TAG_GET_SERIAL, 2U)) { return false; }
    *serial = ((uint64_t)s_mbox[6] << 32) | s_mbox[5];
    return (*serial != 0U);
}
