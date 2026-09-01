/*
 * FlintRTOS - BCM2711 GENET v5 Ethernet MAC driver (Raspberry Pi 4).
 *
 * Implements MDIO/PHY access, MAC setup, and TX/RX via the GENET DMA rings
 * using the DEFAULT queue (ring 16). Register offsets follow the published
 * BCM2711 GENET v5 map (Linux bcmgenet; bare-metal ports).
 *
 * ================================ STATUS ==================================
 * BUILD-VERIFIED, HARDWARE BRING-UP PENDING. This runs with the MMU/caches OFF
 * (see boot/rpi4/start.S), so virtual==physical and no cache maintenance is
 * needed for DMA. On real hardware, validate in this order:
 *   1) MDIO read of the PHY ID at address 1 (BCM54213PE) returns non-0/0xffff.
 *   2) PHY link-up bit.
 *   3) RX descriptor producer index advances when a frame arrives.
 *   4) TX consumer index advances after a send.
 * The RaspberryPi firmware normally enables the GENET clock/power before the
 * kernel runs; if not, a mailbox power-on is required first.
 * ==========================================================================
 */
#include "genet.h"
#include "rpi4.h"
#include "uart.h"

#include <stdint.h>
#include <stdbool.h>

extern void *memcpy(void *, const void *, unsigned long);

/* ---- Register map (offsets from GENET base) ------------------------------ */
#define GENET_BASE              (0xFD580000UL)

#define SYS_REV_CTRL            (0x0000U)
#define SYS_PORT_CTRL           (0x0004U)
#define SYS_RBUF_FLUSH_CTRL     (0x0008U)
#define SYS_TBUF_FLUSH_CTRL     (0x000CU)

#define EXT_RGMII_OOB_CTRL      (0x008CU)
#define  RGMII_LINK             (1U << 4)
#define  OOB_DISABLE            (1U << 5)
#define  RGMII_MODE_EN          (1U << 6)
#define  ID_MODE_DIS            (1U << 16)

#define INTRL2_0_CPU_CLEAR      (0x0208U)
#define INTRL2_0_CPU_MASK_SET   (0x0210U)

#define RBUF_CTRL               (0x0300U)
#define  RBUF_64B_EN            (1U << 0)
#define  RBUF_ALIGN_2B          (1U << 1)
#define RBUF_TBUF_SIZE_CTRL     (0x03B4U)

#define UMAC_CMD                (0x0808U)
#define  CMD_TX_EN              (1U << 0)
#define  CMD_RX_EN              (1U << 1)
#define  CMD_SPEED_1000         (2U << 2)
#define  CMD_SW_RESET           (1U << 13)
#define UMAC_MAC0               (0x080CU)
#define UMAC_MAC1               (0x0810U)
#define UMAC_MAX_FRAME_LEN      (0x0814U)

#define UMAC_MDIO_CMD           (0x0E14U)
#define  MDIO_START_BUSY        (1U << 29)
#define  MDIO_READ_OP           (2U << 26)
#define  MDIO_WRITE_OP          (1U << 26)
#define  MDIO_PMD_SHIFT         (21)
#define  MDIO_REG_SHIFT         (16)

/* DMA blocks. Each ring's registers are at <DMA>+0x1000+ring*0x40; the ring's
   descriptors are at <DMA>+ring_desc_index*0x0C (3 words each). */
#define RDMA_OFFSET             (0x2000U)
#define TDMA_OFFSET             (0x4000U)
#define DMA_RING_CFG            (0x1000U)   /* + within RDMA/TDMA */
#define DMA_CTRL                (0x1004U)
#define DMA_RING_REG_BASE       (0x1040U)   /* ring 0 regs; +ring*0x40 */

/* Per-ring registers (offset from DMA_RING_REG_BASE + ring*0x40). */
#define RDMA_WRITE_PTR          (0x00U)
#define RDMA_PROD_INDEX         (0x08U)
#define RDMA_CONS_INDEX         (0x0CU)
#define DMA_RING_BUF_SIZE       (0x10U)
#define DMA_START_ADDR          (0x14U)
#define DMA_END_ADDR            (0x1CU)
#define DMA_MBUF_DONE_THRESH    (0x24U)
#define TDMA_READ_PTR           (0x00U)
#define TDMA_PROD_INDEX         (0x08U)
#define TDMA_CONS_INDEX         (0x0CU)

/* Descriptor length/status word bits. */
#define DMA_BD_LENGTH_SHIFT     (16)
#define DMA_SOP                 (1U << 13)
#define DMA_EOP                 (1U << 14)
#define DMA_TX_APPEND_CRC       (1U << 6)
#define DMA_OWN                 (1U << 15)

#define DEFAULT_Q               (16U)       /* default DMA queue index */
#define NUM_DESC                (256U)      /* total descriptors per DMA */
#define RX_BUF_COUNT            (16U)
#define TX_BUF_COUNT            (8U)

static inline uint32_t rd(uint32_t off) { return mmio_read32(GENET_BASE + off); }
static inline void     wr(uint32_t off, uint32_t v) { mmio_write32(GENET_BASE + off, v); }

/* DMA buffers (MMU/caches off -> plain memory is DMA-coherent here). */
static uint8_t s_rx_buf[RX_BUF_COUNT][GENET_MAX_FRAME] __attribute__((aligned(64)));
static uint8_t s_tx_buf[TX_BUF_COUNT][GENET_MAX_FRAME] __attribute__((aligned(64)));

static uint32_t s_rx_index;   /* our RX consumer position (descriptor)      */
static uint32_t s_tx_index;   /* our TX producer position (descriptor)      */
static bool     s_link;
static uint32_t s_phy_addr = 1U;   /* discovered by MDIO scan (RPi4 = 1)    */

static void udelay_approx(uint32_t n) { while (n-- > 0U) { __asm__ volatile("nop"); } }

/* ---- MDIO -------------------------------------------------------------- */
static uint16_t mdio_read(uint32_t phy, uint32_t reg)
{
    wr(UMAC_MDIO_CMD, MDIO_START_BUSY | MDIO_READ_OP |
       (phy << MDIO_PMD_SHIFT) | (reg << MDIO_REG_SHIFT));
    /* wait for busy to clear */
    for (uint32_t i = 0U; i < 100000U; i++)
    {
        if ((rd(UMAC_MDIO_CMD) & MDIO_START_BUSY) == 0U) { break; }
    }
    return (uint16_t)(rd(UMAC_MDIO_CMD) & 0xFFFFU);
}

static void mdio_write(uint32_t phy, uint32_t reg, uint16_t val)
{
    wr(UMAC_MDIO_CMD, MDIO_START_BUSY | MDIO_WRITE_OP |
       (phy << MDIO_PMD_SHIFT) | (reg << MDIO_REG_SHIFT) | val);
    for (uint32_t i = 0U; i < 100000U; i++)
    {
        if ((rd(UMAC_MDIO_CMD) & MDIO_START_BUSY) == 0U) { break; }
    }
}

static uint32_t rdma_ring(uint32_t reg);   /* defined below */
static uint32_t tdma_ring(uint32_t reg);

/* Scan MDIO addresses 0..31 for a responding PHY (id reg != 0/0xffff). */
static int mdio_scan(void)
{
    for (uint32_t a = 0U; a < 32U; a++)
    {
        uint16_t id1 = mdio_read(a, 2U);   /* PHYIDR1 */
        if ((id1 != 0x0000U) && (id1 != 0xFFFFU))
        {
            return (int)a;
        }
    }
    return -1;
}

/* One-shot bring-up diagnostics: is GENET powered/addressed, does MDIO reach
 * the PHY, and what is the link state. Print BEFORE trusting the DMA rings. */
void genet_diag(void)
{
    uint32_t rev = rd(SYS_REV_CTRL);
    uart_printf("[genet] SYS_REV_CTRL = %p  (major nibble=%u, expect 6 for GENETv5)\n",
                (void *)(uintptr_t)rev, (unsigned int)((rev >> 24) & 0x0FU));
    uart_printf("[genet] UMAC_CMD     = %p\n", (void *)(uintptr_t)rd(UMAC_CMD));

    int a = mdio_scan();
    if (a < 0)
    {
        uart_printf("[genet] *** no PHY answered MDIO (0..31) ***\n");
        uart_printf("[genet]     -> GENET clock/power likely off, or base/MDIO reg wrong\n");
        return;
    }
    s_phy_addr = (uint32_t)a;
    uint16_t id1  = mdio_read(s_phy_addr, 2U);
    uint16_t id2  = mdio_read(s_phy_addr, 3U);
    uint16_t bmcr = mdio_read(s_phy_addr, 0U);
    uint16_t bmsr = mdio_read(s_phy_addr, 1U);
    uart_printf("[genet] PHY @ addr %u: ID=0x%x:0x%x (BCM54213PE ~ 600d:84xx)\n",
                (unsigned int)s_phy_addr, (unsigned int)id1, (unsigned int)id2);
    uart_printf("[genet]   BMCR=0x%x BMSR=0x%x  link=%s  autoneg=%s\n",
                (unsigned int)bmcr, (unsigned int)bmsr,
                ((bmsr & (1U << 2)) != 0U) ? "UP" : "down",
                ((bmsr & (1U << 5)) != 0U) ? "complete" : "in-progress");
}

/* Dump the DMA ring producer/consumer indices (RX/TX progress on HW). */
void genet_diag_rings(void)
{
    uart_printf("[genet] RX prod=%u cons=%u (ours=%u) | TX prod=%u cons=%u (ours=%u)\n",
                (unsigned int)(rd(rdma_ring(RDMA_PROD_INDEX)) & 0xFFFFU),
                (unsigned int)(rd(rdma_ring(RDMA_CONS_INDEX)) & 0xFFFFU),
                (unsigned int)(s_rx_index & 0xFFFFU),
                (unsigned int)(rd(tdma_ring(TDMA_PROD_INDEX)) & 0xFFFFU),
                (unsigned int)(rd(tdma_ring(TDMA_CONS_INDEX)) & 0xFFFFU),
                (unsigned int)(s_tx_index & 0xFFFFU));
}

/* ---- DMA ring register helpers ---------------------------------------- */
static uint32_t rdma_ring(uint32_t reg) { return RDMA_OFFSET + DMA_RING_REG_BASE + (DEFAULT_Q * 0x40U) + reg; }
static uint32_t tdma_ring(uint32_t reg) { return TDMA_OFFSET + DMA_RING_REG_BASE + (DEFAULT_Q * 0x40U) + reg; }
static uint32_t rx_desc(uint32_t i, uint32_t word) { return RDMA_OFFSET + (i * 0x0CU) + (word * 4U); }
static uint32_t tx_desc(uint32_t i, uint32_t word) { return TDMA_OFFSET + (i * 0x0CU) + (word * 4U); }

static void mac_reset(void)
{
    uint32_t cmd = rd(UMAC_CMD);
    cmd |= CMD_SW_RESET;
    wr(UMAC_CMD, cmd);
    udelay_approx(1000U);
    cmd &= ~CMD_SW_RESET;
    wr(UMAC_CMD, cmd);
    udelay_approx(1000U);
}

static void set_mac_address(const uint8_t mac[6])
{
    wr(UMAC_MAC0, ((uint32_t)mac[0] << 24) | ((uint32_t)mac[1] << 16) |
                  ((uint32_t)mac[2] << 8)  |  (uint32_t)mac[3]);
    wr(UMAC_MAC1, ((uint32_t)mac[4] << 8)  |  (uint32_t)mac[5]);
}

static void rx_ring_init(void)
{
    s_rx_index = 0U;
    /* Point the first RX_BUF_COUNT descriptors at our buffers. */
    for (uint32_t i = 0U; i < RX_BUF_COUNT; i++)
    {
        uint64_t pa = (uint64_t)(uintptr_t)&s_rx_buf[i][0];
        wr(rx_desc(i, 1), (uint32_t)(pa & 0xFFFFFFFFU));       /* addr lo */
        wr(rx_desc(i, 2), (uint32_t)(pa >> 32));               /* addr hi */
        wr(rx_desc(i, 0), 0U);                                 /* status */
    }
    wr(rdma_ring(DMA_START_ADDR), 0U);
    wr(rdma_ring(DMA_END_ADDR), (RX_BUF_COUNT * 0x0CU) / 4U - 1U);
    wr(rdma_ring(DMA_RING_BUF_SIZE), (RX_BUF_COUNT << 16) | GENET_MAX_FRAME);
    wr(rdma_ring(RDMA_PROD_INDEX), 0U);
    wr(rdma_ring(RDMA_CONS_INDEX), 0U);
    wr(rdma_ring(RDMA_WRITE_PTR), 0U);
    wr(rdma_ring(DMA_MBUF_DONE_THRESH), 1U);
    wr(RDMA_OFFSET + DMA_RING_CFG, (1U << DEFAULT_Q));
    wr(RDMA_OFFSET + DMA_CTRL, (1U << 0) | (1U << (DEFAULT_Q + 1)));  /* enable */
}

static void tx_ring_init(void)
{
    s_tx_index = 0U;
    wr(tdma_ring(DMA_START_ADDR), 0U);
    wr(tdma_ring(DMA_END_ADDR), (TX_BUF_COUNT * 0x0CU) / 4U - 1U);
    wr(tdma_ring(DMA_RING_BUF_SIZE), (TX_BUF_COUNT << 16) | GENET_MAX_FRAME);
    wr(tdma_ring(TDMA_PROD_INDEX), 0U);
    wr(tdma_ring(TDMA_CONS_INDEX), 0U);
    wr(tdma_ring(TDMA_READ_PTR), 0U);
    wr(TDMA_OFFSET + DMA_RING_CFG, (1U << DEFAULT_Q));
    wr(TDMA_OFFSET + DMA_CTRL, (1U << 0) | (1U << (DEFAULT_Q + 1)));
}

bool genet_init(const uint8_t mac[6])
{
    /* Flush RBUF/TBUF and reset the MAC. */
    wr(SYS_RBUF_FLUSH_CTRL, 1U);
    udelay_approx(100U);
    wr(SYS_RBUF_FLUSH_CTRL, 0U);
    mac_reset();

    wr(UMAC_MAX_FRAME_LEN, GENET_MAX_FRAME);
    wr(RBUF_CTRL, rd(RBUF_CTRL) | RBUF_ALIGN_2B);
    set_mac_address(mac);

    /* PHY: BCM54213PE (RPi4). Discover its MDIO address, then kick autoneg. */
    {
        int a = mdio_scan();
        if (a >= 0)
        {
            s_phy_addr = (uint32_t)a;
            mdio_write(s_phy_addr, 0U, 0x1200U);  /* BMCR: autoneg enable+restart */
        }
        else
        {
            s_link = false;   /* MDIO silent: GENET clock/power likely off */
        }
    }

    rx_ring_init();
    tx_ring_init();

    /* RGMII out-of-band control: internal delay, RGMII mode. */
    wr(EXT_RGMII_OOB_CTRL, (rd(EXT_RGMII_OOB_CTRL) | RGMII_MODE_EN | OOB_DISABLE) & ~ID_MODE_DIS);

    /* Enable TX + RX at 1 Gbit. */
    wr(UMAC_CMD, rd(UMAC_CMD) | CMD_TX_EN | CMD_RX_EN | CMD_SPEED_1000);

    return true;
}

bool genet_link_up(void)
{
    uint16_t bmsr = mdio_read(s_phy_addr, 1U);   /* BMSR: bit 2 = link up */
    /* BMSR link bit latches low; read twice for the current state. */
    bmsr = mdio_read(s_phy_addr, 1U);
    s_link = ((bmsr & (1U << 2)) != 0U);
    return s_link;
}

bool genet_send(const uint8_t *frame, uint16_t len)
{
    if (len > GENET_MAX_FRAME) { return false; }

    uint32_t i = s_tx_index % TX_BUF_COUNT;
    uint64_t pa = (uint64_t)(uintptr_t)&s_tx_buf[i][0];

    (void)memcpy(&s_tx_buf[i][0], frame, len);

    wr(tx_desc(i, 1), (uint32_t)(pa & 0xFFFFFFFFU));
    wr(tx_desc(i, 2), (uint32_t)(pa >> 32));
    wr(tx_desc(i, 0), ((uint32_t)len << DMA_BD_LENGTH_SHIFT) |
                      DMA_SOP | DMA_EOP | DMA_TX_APPEND_CRC);

    s_tx_index++;
    wr(tdma_ring(TDMA_PROD_INDEX), s_tx_index & 0xFFFFU);
    return true;
}

bool genet_recv(uint8_t *buf, uint16_t *len)
{
    uint32_t prod = rd(rdma_ring(RDMA_PROD_INDEX)) & 0xFFFFU;
    if ((s_rx_index & 0xFFFFU) == prod)
    {
        return false;                        /* nothing new */
    }

    uint32_t i = s_rx_index % RX_BUF_COUNT;
    uint32_t status = rd(rx_desc(i, 0));
    uint16_t rlen = (uint16_t)((status >> DMA_BD_LENGTH_SHIFT) & 0x0FFFU);

    if (rlen > *len) { rlen = *len; }
    (void)memcpy(buf, &s_rx_buf[i][0], rlen);
    *len = rlen;

    s_rx_index++;
    wr(rdma_ring(RDMA_CONS_INDEX), s_rx_index & 0xFFFFU);
    return true;
}
