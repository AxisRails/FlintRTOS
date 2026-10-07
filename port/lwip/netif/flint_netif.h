/*
 * FlintRTOS - lwIP netif for Raspberry Pi 4 (BCM2711 GENET).
 */
#ifndef FLINT_NETIF_H
#define FLINT_NETIF_H

#include "lwip/netif.h"
#include "lwip/err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pass to netif_add() as the init callback. */
err_t flint_netif_init(struct netif *netif);

/* Poll for received frames and push them into lwIP (call from the main loop or
   a task; a GENET RX interrupt will drive this once the driver lands). */
void  flint_netif_poll(struct netif *netif);

/* Driver timestamps in ARM generic-timer ticks (CNTPCT). rx: the frame being
   delivered right now (valid inside a NO_SYS receive callback). tx: the most
   recently transmitted frame (read right after a NO_SYS udp_sendto()). */
uint64_t flint_netif_rx_stamp(void);
uint64_t flint_netif_tx_stamp(void);

#ifdef __cplusplus
}
#endif

#endif /* FLINT_NETIF_H */
