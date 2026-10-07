/*
 * FlintRTOS - lwIP netif for Raspberry Pi 4 (BCM2711 GENET) - bring-up skeleton.
 *
 * Wires an Ethernet netif into lwIP with the correct output paths (ARP for v4,
 * ethip6 for v6) on top of the GENET driver (validated on hardware: DHCP).
 * RX is polled by flint_netif_poll(); each frame carries a driver timestamp
 * taken in the GENET RX interrupt, exposed for PTP via flint_netif_rx_stamp().
 */
#include "flint_netif.h"

#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/etharp.h"
#include "netif/ethernet.h"
#if LWIP_IPV6
#include "lwip/ethip6.h"
#endif

#include "genet.h"
#include "mailbox.h"
#include "uart.h"
#include <stdbool.h>
#include <stdint.h>

#define FLINT_NETIF_MTU   (1500)

/* Fallback locally-administered MAC, used only if the GPU firmware doesn't
   report the board's factory address over the mailbox. */
static const uint8_t k_mac[6] = { 0x02U, 0x00U, 0x5EU, 0x00U, 0x00U, 0x01U };

/* RX timestamp (CNTPCT ticks) of the frame currently being handed to lwIP.
   In NO_SYS mode netif->input runs synchronously down to the UDP callback,
   so a PTP receive handler can read it with flint_netif_rx_stamp(). */
static uint64_t s_rx_stamp;

uint64_t flint_netif_rx_stamp(void) { return s_rx_stamp; }
uint64_t flint_netif_tx_stamp(void) { return genet_last_tx_stamp(); }

/* Contiguous scratch to gather a (possibly chained) pbuf before DMA. */
static uint8_t s_tx_gather[GENET_MAX_FRAME];

/* Low-level transmit: hand a fully-formed Ethernet frame to the GENET MAC. */
static err_t flint_low_level_output(struct netif *netif, struct pbuf *p)
{
    (void)netif;
    if (p->tot_len > sizeof(s_tx_gather))
    {
        return ERR_BUF;
    }
    (void)pbuf_copy_partial(p, s_tx_gather, p->tot_len, 0);
    if (!genet_send(s_tx_gather, p->tot_len))
    {
        return ERR_IF;
    }
    return ERR_OK;
}

err_t flint_netif_init(struct netif *netif)
{
    netif->name[0] = 'e';
    netif->name[1] = 'n';

    netif->output     = etharp_output;     /* IPv4 -> ARP -> linkoutput */
#if LWIP_IPV6
    netif->output_ip6 = ethip6_output;      /* IPv6 -> ND  -> linkoutput */
#endif
    netif->linkoutput = flint_low_level_output;

    netif->mtu        = FLINT_NETIF_MTU;
    netif->hwaddr_len = ETH_HWADDR_LEN;
    {
        uint8_t mac[6];
        bool    fw = mbox_get_mac(mac);
        for (int i = 0; i < ETH_HWADDR_LEN; i++)
        {
            netif->hwaddr[i] = fw ? mac[i] : k_mac[i];
        }
        uart_printf("[netif] MAC %x:%x:%x:%x:%x:%x (%s)\n",
                    netif->hwaddr[0], netif->hwaddr[1], netif->hwaddr[2],
                    netif->hwaddr[3], netif->hwaddr[4], netif->hwaddr[5],
                    fw ? "board MAC from firmware" : "fallback - mailbox did not answer");
    }
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET
                 | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP
#if LWIP_IGMP
                 | NETIF_FLAG_IGMP        /* PTP multicast 224.0.1.129 */
#endif
                 ;

    /* Bring the MAC up with our station address. */
    (void)genet_init(netif->hwaddr);

    return ERR_OK;
}

void flint_netif_poll(struct netif *netif)
{
    uint8_t  frame[GENET_MAX_FRAME];
    uint16_t len;

    /* Drain the RX ring; wrap each frame in a pbuf and hand it to lwIP. */
    for (;;)
    {
        len = (uint16_t)sizeof(frame);
        if (!genet_recv(frame, &len, &s_rx_stamp))
        {
            break;   /* ring empty */
        }
        struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
        if (p != NULL)
        {
            (void)pbuf_take(p, frame, len);
            if (netif->input(p, netif) != ERR_OK)
            {
                pbuf_free(p);
            }
        }
    }
}
