/*
 * FlintRTOS - lwIP netif for Raspberry Pi 4 (BCM2711 GENET) - bring-up skeleton.
 *
 * Wires an Ethernet netif into lwIP with the correct output paths (ARP for v4,
 * ethip6 for v6). The low-level frame TX/RX against the GENET MAC is stubbed
 * and is the next hardware task: program the GENET descriptors/DMA, drive RX
 * from its interrupt into flint_netif_poll(). Until then the netif links up
 * but transmits/receives nothing (safe no-op), so the stack can be exercised
 * against a loopback or in emulation.
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
#include <stdint.h>

#define FLINT_NETIF_MTU   (1500)

/* Locally-administered placeholder MAC (replace with the Pi's real address,
   read from the VideoCore mailbox, in a later revision). */
static const uint8_t k_mac[6] = { 0x02U, 0x00U, 0x5EU, 0x00U, 0x00U, 0x01U };

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
    for (int i = 0; i < ETH_HWADDR_LEN; i++)
    {
        netif->hwaddr[i] = k_mac[i];
    }
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET
                 | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;

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
        if (!genet_recv(frame, &len))
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
