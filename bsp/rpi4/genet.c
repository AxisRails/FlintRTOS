/*
 * FlintRTOS - BCM2711 GENET v5 Ethernet MAC driver (Raspberry Pi 4).
 *
 * Init sequence realigned to the known-good U-Boot bcmgenet driver: full UMAC
 * reset, MIB clear, RBUF setup, disable-DMA -> ring init -> enable-DMA ordering,
 * a 256-entry RX ring whose descriptors are armed with (buf_len<<16)|DMA_OWN,
 * and rgmii-rxid (PHY provides RX delay, MAC adds none). MMU maps RAM as Normal
 * Non-Cacheable, so DMA is coherent without cache maintenance.
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
#define  PORT_MODE_EXT_GPHY     (3U)          /* external RGMII gigabit PHY */
#define  LED_ACT_SOURCE_MAC     (1U << 9)
#define SYS_RBUF_FLUSH_CTRL     (0x0008U)
#define SYS_TBUF_FLUSH_CTRL     (0x000CU)

#define EXT_RGMII_OOB_CTRL      (0x008CU)
#define  RGMII_LINK             (1U << 4)
#define  OOB_DISABLE            (1U << 5)
#define  RGMII_MODE_EN          (1U << 6)
#define  ID_MODE_DIS            (1U << 16)

#define RBUF_CTRL               (0x0300U)
#define  RBUF_ALIGN_2B          (1U << 1)
#define RBUF_CHK_CTRL           (0x0314U)     /* RX checksum offload ctrl */
#define RBUF_TBUF_SIZE_CTRL     (0x03B4U)

/* Hardware Filter Block (GENET v5: hfb_reg_offset = 0xFC00). */
#define HFB_CTRL                (0xFC00U)
#define HFB_FLT_ENABLE_V3PLUS   (0xFC10U)

#define UMAC_CMD                (0x0808U)
#define  CMD_TX_EN              (1U << 0)
#define  CMD_RX_EN              (1U << 1)
#define  CMD_SPEED_SHIFT        (2)
#define  CMD_SPEED_1000         (2U << CMD_SPEED_SHIFT)
#define  CMD_SPEED_100          (1U << CMD_SPEED_SHIFT)
#define  CMD_SPEED_MASK         (3U << CMD_SPEED_SHIFT)
#define  CMD_PROMISC            (1U << 4)
#define  CMD_SW_RESET           (1U << 13)
#define  CMD_LCL_LOOP_EN        (1U << 15)
#define UMAC_MAC0               (0x080CU)
#define UMAC_MAC1               (0x0810U)
#define UMAC_MAX_FRAME_LEN      (0x0814U)
#define UMAC_TX_FLUSH           (0x0B34U)
#define UMAC_MIB_CTRL           (0x0D80U)
#define UMAC_MDF_CTRL           (0x0E50U)     /* MAC destination filter enable */
#define  MIB_RESET_RX           (1U << 0)
#define  MIB_RESET_RUNT         (1U << 1)
#define  MIB_RESET_TX           (1U << 2)

#define UMAC_MDIO_CMD           (0x0E14U)
#define  MDIO_START_BUSY        (1U << 29)
#define  MDIO_READ_OP           (2U << 26)
#define  MDIO_WRITE_OP          (1U << 26)
#define  MDIO_PMD_SHIFT         (21)
#define  MDIO_REG_SHIFT         (16)

/* DMA block layout (see notes in the previous revision - verified on HW). */
#define RDMA_OFFSET             (0x2000U)
#define TDMA_OFFSET             (0x4000U)
#define DMA_DESC_SIZE           (0x0CU)          /* 3 words per descriptor */
#define DMA_RING_REG_BASE       (0x0C00U)        /* = TOTAL_DESC * DMA_DESC_SIZE */
#define DMA_RING_CFG            (0x1040U)        /* global */
#define DMA_CTRL                (0x1044U)        /* global: DMA_EN + ring buf en */
#define DMA_STATUS_REG          (0x1048U)
#define DMA_SCB_BURST_SIZE      (0x104CU)
#define  DMA_MAX_BURST_LENGTH   (0x08U)
#define  DMA_EN                 (1U << 0)
#define  DMA_RING_BUF_EN_SHIFT  (0x01U)

/* Per-ring registers (offset from ring base). NOTE: TDMA swaps PROD/CONS. */
#define RDMA_WRITE_PTR          (0x00U)
#define RDMA_PROD_INDEX         (0x08U)
#define RDMA_CONS_INDEX         (0x0CU)
#define TDMA_READ_PTR           (0x00U)
#define TDMA_CONS_INDEX         (0x08U)          /* HW-managed */
#define TDMA_PROD_INDEX         (0x0CU)          /* CPU writes to kick TX */
#define DMA_RING_BUF_SIZE       (0x10U)
#define DMA_START_ADDR          (0x14U)
#define DMA_END_ADDR            (0x1CU)
#define DMA_MBUF_DONE_THRESH    (0x24U)
#define DMA_FLOW_PERIOD         (0x28U)          /* TDMA */
#define RDMA_XON_XOFF_THRESH    (0x28U)          /* RDMA: RX flow-control thresholds */
#define RDMA_READ_PTR           (0x2CU)          /* RDMA (aliases TDMA_WRITE_PTR) */
#define DMA_RING_SIZE_SHIFT     (16)
#define DMA_FC_THRESH_LO        (5U)
#define DMA_FC_THRESH_HI        (256U >> 4)      /* TOTAL_DESC >> 4 */
#define DMA_XOFF_THRESHOLD_SHIFT (16)

/* BCM54xx PHY shadow/auxiliary registers (Linux broadcom.c / bcm-phy-lib). */
#define MII_BCM54XX_AUX_CTL              (0x18U)
#define  AUXCTL_SHDWSEL_MISC             (0x07U)
#define  AUXCTL_MISC_WREN                (0x8000U)
#define  AUXCTL_MISC_RGMII_SKEW_EN       (0x0100U)   /* RGMII RXC-RXD skew */
#define MII_BCM54XX_SHD                  (0x1CU)
#define  SHD_WRITE                       (0x8000U)
#define  BCM54810_SHD_CLK_CTL            (0x03U)
#define  BCM54810_SHD_CLK_CTL_GTXCLK_EN  (1U << 9)   /* PHY internal TX clk delay */

/* Descriptor length/status word bits. */
#define DMA_BD_LENGTH_SHIFT     (16)
#define DMA_BD_LENGTH_MASK      (0x0FFFU)
#define DMA_TX_APPEND_CRC       (1U << 6)
#define DMA_TX_QTAG_SHIFT       (7)
#define DMA_TX_QTAG             (0x3FU << DMA_TX_QTAG_SHIFT)
#define DMA_SOP                 (1U << 13)
#define DMA_EOP                 (1U << 14)
/* RX status low bits (Linux bcmgenet): OV, CRC_ERROR, RXER, NO, LG */
#define DMA_RX_ERR_MASK         (0x1FU)
/* RBUF_ALIGN_2B pad in front of every received frame. */
#define RX_BUF_OFFSET           (2U)
#define DMA_OWN                 (1U << 15)

#define DEFAULT_Q               (16U)
#define TOTAL_DESC              (256U)
#define RX_DESCS                (256U)
#define TX_DESCS                (256U)
#define RX_BUF_LENGTH           (2048U)
#define ENET_MAX_MTU_SIZE       (1536U)

static inline uint32_t rd(uint32_t off) { return mmio_read32(GENET_BASE + off); }
static inline void     wr(uint32_t off, uint32_t v) { mmio_write32(GENET_BASE + off, v); }
static inline void     dma_barrier(void) { __asm__ volatile("dsb sy" ::: "memory"); }

/* DMA buffers (Normal Non-Cacheable RAM -> DMA-coherent, no cache ops). */
static uint8_t s_rx_buf[RX_DESCS][RX_BUF_LENGTH] __attribute__((aligned(64)));
static uint8_t s_tx_buf[32][RX_BUF_LENGTH]       __attribute__((aligned(64)));

static uint32_t s_rx_dbg  = 0U;
static uint32_t s_rx_drop = 0U;
static uint32_t s_rx_cons;    /* our RX consumer index (frames read)   */
static uint32_t s_tx_prod;    /* our TX producer index (frames queued) */
static bool     s_link;
static uint32_t s_phy_addr = 1U;

static void udelay_approx(uint32_t n) { while (n-- > 0U) { __asm__ volatile("nop"); } }

/* ---- MDIO ---------------------------------------------------------------- */
static uint16_t mdio_read(uint32_t phy, uint32_t reg)
{
    wr(UMAC_MDIO_CMD, MDIO_START_BUSY | MDIO_READ_OP |
       (phy << MDIO_PMD_SHIFT) | (reg << MDIO_REG_SHIFT));
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

static int mdio_scan(void)
{
    for (uint32_t a = 0U; a < 32U; a++)
    {
        uint16_t id1 = mdio_read(a, 2U);
        if ((id1 != 0x0000U) && (id1 != 0xFFFFU)) { return (int)a; }
    }
    return -1;
}

/* BCM54xx shadow register (reg 0x1C) access - Linux bcm_phy_read/write_shadow. */
static uint16_t phy_read_shadow(uint32_t phy, uint16_t shd)
{
    mdio_write(phy, MII_BCM54XX_SHD, (uint16_t)((shd & 0x1FU) << 10));
    return (uint16_t)(mdio_read(phy, MII_BCM54XX_SHD) & 0x3FFU);
}

static void phy_write_shadow(uint32_t phy, uint16_t shd, uint16_t val)
{
    mdio_write(phy, MII_BCM54XX_SHD,
               (uint16_t)(SHD_WRITE | ((shd & 0x1FU) << 10) | (val & 0x3FFU)));
}

/* Configure the BCM54213 RGMII delays for rgmii-rxid, exactly as Linux
 * bcm54xx_config_clock_delay() does:
 *   - RX: ENABLE the PHY's RGMII RXC-RXD skew (PHY provides the RX delay),
 *   - TX: DISABLE the PHY's internal GTXCLK delay (GENET's ID mode adds the
 *     TX delay instead; having both would double-delay TX and corrupt it). */
static void phy_config_rgmii_delays(uint32_t phy)
{
    /* RX skew via Auxiliary Control MISC shadow. */
    mdio_write(phy, MII_BCM54XX_AUX_CTL,
               (uint16_t)(AUXCTL_SHDWSEL_MISC | (AUXCTL_SHDWSEL_MISC << 12)));
    uint16_t misc = mdio_read(phy, MII_BCM54XX_AUX_CTL);
    misc |= AUXCTL_MISC_WREN | AUXCTL_MISC_RGMII_SKEW_EN;
    mdio_write(phy, MII_BCM54XX_AUX_CTL, (uint16_t)(AUXCTL_SHDWSEL_MISC | misc));

    /* TX: clear GTXCLK delay in Clock Alignment Control (shadow 0x03). */
    uint16_t clk = phy_read_shadow(phy, BCM54810_SHD_CLK_CTL);
    uint16_t clk_before = clk;
    clk &= (uint16_t)~BCM54810_SHD_CLK_CTL_GTXCLK_EN;
    phy_write_shadow(phy, BCM54810_SHD_CLK_CTL, clk);
    uint16_t clk_after = phy_read_shadow(phy, BCM54810_SHD_CLK_CTL);

    mdio_write(phy, MII_BCM54XX_AUX_CTL,
               (uint16_t)(AUXCTL_SHDWSEL_MISC | (AUXCTL_SHDWSEL_MISC << 12)));
    uart_printf("[genet] PHY delays: MISC=0x%x (skew bit8 %s) | CLK_CTL 0x%x -> 0x%x (GTXCLK %s)\n",
                (unsigned int)mdio_read(phy, MII_BCM54XX_AUX_CTL),
                ((mdio_read(phy, MII_BCM54XX_AUX_CTL) & AUXCTL_MISC_RGMII_SKEW_EN) != 0U) ? "ON" : "off",
                (unsigned int)clk_before, (unsigned int)clk_after,
                ((clk_after & BCM54810_SHD_CLK_CTL_GTXCLK_EN) != 0U) ? "STILL ON" : "off");
}

/* ---- Ring register helpers ---------------------------------------------- */
static uint32_t rdma_ring(uint32_t reg) { return RDMA_OFFSET + DMA_RING_REG_BASE + (DEFAULT_Q * 0x40U) + reg; }
static uint32_t tdma_ring(uint32_t reg) { return TDMA_OFFSET + DMA_RING_REG_BASE + (DEFAULT_Q * 0x40U) + reg; }
static uint32_t rx_desc(uint32_t i, uint32_t word) { return RDMA_OFFSET + (i * DMA_DESC_SIZE) + (word * 4U); }
static uint32_t tx_desc(uint32_t i, uint32_t word) { return TDMA_OFFSET + (i * DMA_DESC_SIZE) + (word * 4U); }

/* ---- Diagnostics --------------------------------------------------------- */
void genet_diag(void)
{
    uint32_t rev = rd(SYS_REV_CTRL);
    uart_printf("[genet] SYS_REV_CTRL = %p  (major nibble=%u, expect 6)\n",
                (void *)(uintptr_t)rev, (unsigned int)((rev >> 24) & 0x0FU));
    int a = mdio_scan();
    if (a < 0) { uart_printf("[genet] *** no PHY answered MDIO ***\n"); return; }
    s_phy_addr = (uint32_t)a;
    uint16_t bmsr = mdio_read(s_phy_addr, 1U);
    uart_printf("[genet] PHY @ %u: ID=0x%x:0x%x BMSR=0x%x link=%s\n",
                (unsigned int)s_phy_addr, (unsigned int)mdio_read(s_phy_addr, 2U),
                (unsigned int)mdio_read(s_phy_addr, 3U), (unsigned int)bmsr,
                ((bmsr & (1U << 2)) != 0U) ? "UP" : "down");
}

void genet_diag_rings(void)
{
    uart_printf("[genet] RX prod=%u cons=%u (ours=%u) | TX prod=%u cons=%u (ours=%u)\n",
                (unsigned int)(rd(rdma_ring(RDMA_PROD_INDEX)) & 0xFFFFU),
                (unsigned int)(rd(rdma_ring(RDMA_CONS_INDEX)) & 0xFFFFU),
                (unsigned int)(s_rx_cons & 0xFFFFU),
                (unsigned int)(rd(tdma_ring(TDMA_PROD_INDEX)) & 0xFFFFU),
                (unsigned int)(rd(tdma_ring(TDMA_CONS_INDEX)) & 0xFFFFU),
                (unsigned int)(s_tx_prod & 0xFFFFU));
}

void genet_probe(void)
{
    uart_printf("[probe] TDMA CTRL=%p STATUS=%p | RDMA CTRL=%p STATUS=%p\n",
                (void *)(uintptr_t)rd(TDMA_OFFSET + DMA_CTRL),
                (void *)(uintptr_t)rd(TDMA_OFFSET + DMA_STATUS_REG),
                (void *)(uintptr_t)rd(RDMA_OFFSET + DMA_CTRL),
                (void *)(uintptr_t)rd(RDMA_OFFSET + DMA_STATUS_REG));
    uart_printf("[probe] UMAC_CMD=%p RBUF_CTRL=%p\n",
                (void *)(uintptr_t)rd(UMAC_CMD), (void *)(uintptr_t)rd(RBUF_CTRL));
    uart_printf("[probe] RX desc0: sts=%p addr_lo=%p | RX ringbufsz=%p end=%p\n",
                (void *)(uintptr_t)rd(rx_desc(0U, 0U)),
                (void *)(uintptr_t)rd(rx_desc(0U, 1U)),
                (void *)(uintptr_t)rd(rdma_ring(DMA_RING_BUF_SIZE)),
                (void *)(uintptr_t)rd(rdma_ring(DMA_END_ADDR)));
}

/* ---- Reset / DMA enable (U-Boot ordering) -------------------------------- */
static void umac_reset(void)
{
    uint32_t reg = rd(SYS_RBUF_FLUSH_CTRL);
    reg |= (1U << 1);
    wr(SYS_RBUF_FLUSH_CTRL, reg);
    udelay_approx(1000U);
    reg &= ~(1U << 1);
    wr(SYS_RBUF_FLUSH_CTRL, reg);
    udelay_approx(1000U);
    wr(SYS_RBUF_FLUSH_CTRL, 0U);
    udelay_approx(1000U);

    wr(UMAC_CMD, 0U);
    wr(UMAC_CMD, CMD_SW_RESET | CMD_LCL_LOOP_EN);
    udelay_approx(200U);
    wr(UMAC_CMD, 0U);

    wr(UMAC_MIB_CTRL, MIB_RESET_RX | MIB_RESET_TX | MIB_RESET_RUNT);
    wr(UMAC_MIB_CTRL, 0U);

    wr(UMAC_MAX_FRAME_LEN, ENET_MAX_MTU_SIZE);

    reg = rd(RBUF_CTRL);
    reg |= RBUF_ALIGN_2B;
    wr(RBUF_CTRL, reg);
    wr(RBUF_TBUF_SIZE_CTRL, 1U);

    /* Nothing may silently drop RX: disable RX checksum offload, the MAC
     * destination filter (we run promiscuous), and the hardware filter block. */
    wr(RBUF_CHK_CTRL, 0U);
    wr(UMAC_MDF_CTRL, 0U);
    wr(HFB_CTRL, 0U);
    wr(HFB_FLT_ENABLE_V3PLUS, 0U);
    wr(HFB_FLT_ENABLE_V3PLUS + 4U, 0U);
}

static void set_mac_address(const uint8_t mac[6])
{
    wr(UMAC_MAC0, ((uint32_t)mac[0] << 24) | ((uint32_t)mac[1] << 16) |
                  ((uint32_t)mac[2] << 8)  |  (uint32_t)mac[3]);
    wr(UMAC_MAC1, ((uint32_t)mac[4] << 8)  |  (uint32_t)mac[5]);
}

static void disable_dma(void)
{
    uint32_t reg = rd(TDMA_OFFSET + DMA_CTRL);
    reg &= ~DMA_EN;
    wr(TDMA_OFFSET + DMA_CTRL, reg);
    reg = rd(RDMA_OFFSET + DMA_CTRL);
    reg &= ~DMA_EN;
    wr(RDMA_OFFSET + DMA_CTRL, reg);

    wr(UMAC_TX_FLUSH, 1U);
    udelay_approx(1000U);
    wr(UMAC_TX_FLUSH, 0U);
}

static void enable_dma(void)
{
    uint32_t dma_ctrl = (1U << (DEFAULT_Q + DMA_RING_BUF_EN_SHIFT)) | DMA_EN;
    wr(TDMA_OFFSET + DMA_CTRL, dma_ctrl);
    wr(RDMA_OFFSET + DMA_CTRL, dma_ctrl);
}

static void rx_ring_init(void)
{
    uint32_t i;
    s_rx_cons = 0U;

    wr(RDMA_OFFSET + DMA_SCB_BURST_SIZE, DMA_MAX_BURST_LENGTH);

    /* Point every RX descriptor at its buffer. As in Linux bcmgenet_rx_refill,
     * only the ADDRESS is set - the hardware writes the length/status word. */
    for (i = 0U; i < RX_DESCS; i++)
    {
        uint64_t pa = (uint64_t)(uintptr_t)&s_rx_buf[i][0];
        wr(rx_desc(i, 1), (uint32_t)(pa & 0xFFFFFFFFU));
        wr(rx_desc(i, 2), (uint32_t)(pa >> 32));
    }

    wr(rdma_ring(RDMA_PROD_INDEX), 0U);
    wr(rdma_ring(RDMA_CONS_INDEX), 0U);
    wr(rdma_ring(DMA_RING_BUF_SIZE), (RX_DESCS << DMA_RING_SIZE_SHIFT) | RX_BUF_LENGTH);
    wr(rdma_ring(RDMA_XON_XOFF_THRESH),
       (DMA_FC_THRESH_LO << DMA_XOFF_THRESHOLD_SHIFT) | DMA_FC_THRESH_HI);
    wr(rdma_ring(DMA_START_ADDR), 0U);
    wr(rdma_ring(DMA_START_ADDR + 4U), 0U);
    wr(rdma_ring(RDMA_READ_PTR), 0U);
    wr(rdma_ring(RDMA_WRITE_PTR), 0U);
    wr(rdma_ring(RDMA_WRITE_PTR + 4U), 0U);
    wr(rdma_ring(DMA_END_ADDR), (RX_DESCS * DMA_DESC_SIZE / 4U) - 1U);
    wr(rdma_ring(DMA_END_ADDR + 4U), 0U);
    wr(rdma_ring(DMA_MBUF_DONE_THRESH), 1U);

    wr(RDMA_OFFSET + DMA_RING_CFG, (1U << DEFAULT_Q));
}

static void tx_ring_init(void)
{
    s_tx_prod = 0U;

    wr(TDMA_OFFSET + DMA_SCB_BURST_SIZE, DMA_MAX_BURST_LENGTH);

    wr(tdma_ring(TDMA_READ_PTR), 0U);
    wr(tdma_ring(TDMA_READ_PTR + 4U), 0U);
    wr(tdma_ring(TDMA_PROD_INDEX), 0U);
    wr(tdma_ring(TDMA_CONS_INDEX), 0U);
    wr(tdma_ring(DMA_RING_BUF_SIZE), (TX_DESCS << DMA_RING_SIZE_SHIFT) | RX_BUF_LENGTH);
    wr(tdma_ring(DMA_START_ADDR), 0U);
    wr(tdma_ring(DMA_START_ADDR + 4U), 0U);
    wr(tdma_ring(DMA_END_ADDR), (TX_DESCS * DMA_DESC_SIZE / 4U) - 1U);
    wr(tdma_ring(DMA_END_ADDR + 4U), 0U);
    wr(tdma_ring(DMA_MBUF_DONE_THRESH), 1U);
    wr(tdma_ring(DMA_FLOW_PERIOD), 0U);

    wr(TDMA_OFFSET + DMA_RING_CFG, (1U << DEFAULT_Q));
}

bool genet_init(const uint8_t mac[6])
{
    umac_reset();

    /* Route the GENET core to the EXTERNAL RGMII gigabit PHY. Without this the
     * MAC/DMA run but no data crosses the RGMII pins to the BCM54213. */
    wr(SYS_PORT_CTRL, PORT_MODE_EXT_GPHY);

    set_mac_address(mac);

    /* Promiscuous for bring-up (accept all frames) + keep TX/RX disabled here. */
    wr(UMAC_CMD, rd(UMAC_CMD) | CMD_PROMISC);

    /* PHY discovery, RGMII delay config, then kick auto-negotiation. */
    {
        int a = mdio_scan();
        if (a >= 0)
        {
            s_phy_addr = (uint32_t)a;
            phy_config_rgmii_delays(s_phy_addr);   /* RX skew on, GTXCLK off */
            mdio_write(s_phy_addr, 0U, 0x1200U);   /* BMCR: autoneg enable+restart */
        }
        else { s_link = false; }
    }

    disable_dma();
    rx_ring_init();
    tx_ring_init();
    enable_dma();

    /* rgmii-rxid, per Linux bcmgenet_mii_config: clear OOB_DISABLE, enable
     * RGMII, and CLEAR ID_MODE_DIS so GENET's ID mode adds the TX clock delay
     * (the PHY's RX skew supplies the RX delay - different paths, not double). */
    {
        uint32_t reg = rd(EXT_RGMII_OOB_CTRL);
        reg &= ~OOB_DISABLE;
        reg &= ~ID_MODE_DIS;
        reg |= RGMII_MODE_EN;
        wr(EXT_RGMII_OOB_CTRL, reg);
    }

    /* Enable TX + RX (speed refined at link-up). */
    wr(UMAC_CMD, rd(UMAC_CMD) | CMD_TX_EN | CMD_RX_EN | CMD_SPEED_1000);
    dma_barrier();

    return true;
}

bool genet_link_up(void)
{
    uint16_t bmsr = mdio_read(s_phy_addr, 1U);
    bmsr = mdio_read(s_phy_addr, 1U);
    s_link = ((bmsr & (1U << 2)) != 0U);
    return s_link;
}

void genet_adjust_link(void)
{
    uint16_t aux = mdio_read(s_phy_addr, 0x19U);
    uint32_t hcd = ((uint32_t)aux >> 8) & 0x7U;
    uint32_t speed_bits;
    unsigned int mbps;

    switch (hcd)
    {
        case 7U: case 6U: speed_bits = CMD_SPEED_1000; mbps = 1000U; break;
        case 5U: case 3U: speed_bits = CMD_SPEED_100;  mbps = 100U;  break;
        default:          speed_bits = 0U;             mbps = 10U;   break;
    }

    uint32_t cmd = rd(UMAC_CMD);
    cmd &= ~CMD_SPEED_MASK;
    cmd |= speed_bits | CMD_TX_EN | CMD_RX_EN;
    wr(UMAC_CMD, cmd);

    uint32_t reg = rd(EXT_RGMII_OOB_CTRL);
    reg &= ~OOB_DISABLE;
    reg &= ~ID_MODE_DIS;                 /* rgmii-rxid: GENET supplies TX delay */
    reg |= RGMII_LINK | RGMII_MODE_EN;
    wr(EXT_RGMII_OOB_CTRL, reg);
    dma_barrier();

    uart_printf("[genet] link up: aux=0x%x -> %u Mbps, UMAC_CMD=0x%x OOB=0x%x\n",
                (unsigned int)aux, mbps, (unsigned int)rd(UMAC_CMD),
                (unsigned int)rd(EXT_RGMII_OOB_CTRL));
}

bool genet_send(const uint8_t *frame, uint16_t len)
{
    if (len > RX_BUF_LENGTH) { return false; }

    uint32_t di = s_tx_prod % TX_DESCS;   /* descriptor index tracks prod index */
    uint32_t bi = s_tx_prod % 32U;        /* buffer pool index                  */
    uint64_t pa = (uint64_t)(uintptr_t)&s_tx_buf[bi][0];

    (void)memcpy(&s_tx_buf[bi][0], frame, len);

    uint32_t len_stat = ((uint32_t)len << DMA_BD_LENGTH_SHIFT) |
                        DMA_TX_QTAG | DMA_SOP | DMA_EOP | DMA_TX_APPEND_CRC;
    wr(tx_desc(di, 1), (uint32_t)(pa & 0xFFFFFFFFU));
    wr(tx_desc(di, 2), (uint32_t)(pa >> 32));
    wr(tx_desc(di, 0), len_stat);

    dma_barrier();
    s_tx_prod++;
    wr(tdma_ring(TDMA_PROD_INDEX), s_tx_prod & 0xFFFFU);
    dma_barrier();
    return true;
}

bool genet_recv(uint8_t *buf, uint16_t *len)
{
    for (;;)
    {
        dma_barrier();
        uint32_t prod = rd(rdma_ring(RDMA_PROD_INDEX)) & 0xFFFFU;
        if ((s_rx_cons & 0xFFFFU) == prod)
        {
            return false;                    /* nothing new */
        }

        uint32_t i = s_rx_cons % RX_DESCS;
        uint32_t status = rd(rx_desc(i, 0));
        uint16_t rlen = (uint16_t)((status >> DMA_BD_LENGTH_SHIFT) & DMA_BD_LENGTH_MASK);

        /* Hand the buffer back to the ring whatever we decide about the frame. */
        s_rx_cons++;
        wr(rdma_ring(RDMA_CONS_INDEX), s_rx_cons & 0xFFFFU);
        dma_barrier();

        /* Drop fragments (no SOP/EOP) and frames the MAC flagged as bad. */
        if (((status & (DMA_SOP | DMA_EOP)) != (DMA_SOP | DMA_EOP)) ||
            ((status & DMA_RX_ERR_MASK) != 0U) ||
            (rlen <= RX_BUF_OFFSET))
        {
            s_rx_drop++;
            if (s_rx_drop <= 4U)
            {
                uart_printf("[rx] DROP sts=0x%x len=%u\n", (unsigned int)status, (unsigned int)rlen);
            }
            continue;                        /* try the next descriptor */
        }

        /*
         * RBUF_ALIGN_2B: the MAC writes the frame 2 bytes into the buffer so
         * the IP header lands 4-byte aligned, and the descriptor length counts
         * that pad. Skip it, or lwIP sees a MAC shifted by two bytes.
         */
        rlen = (uint16_t)(rlen - RX_BUF_OFFSET);
        if (rlen > *len) { rlen = *len; }
        (void)memcpy(buf, &s_rx_buf[i][RX_BUF_OFFSET], rlen);
        *len = rlen;

        if (s_rx_dbg < 8U)
        {
            s_rx_dbg++;
            uart_printf("[rx] #%u len=%u sts=0x%x dst=%x:%x:%x:%x:%x:%x type=0x%x%x\n",
                        (unsigned int)s_rx_dbg, (unsigned int)rlen, (unsigned int)status,
                        buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[12], buf[13]);
        }
        return true;
    }
}
