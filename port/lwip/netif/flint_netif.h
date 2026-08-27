/*
 * FlintRTOS - lwIP netif for Raspberry Pi 4.
 * Bring-up skeleton: presents an Ethernet netif to lwIP. The low-level TX/RX
 * against the BCM2711 GENET MAC is the next hardware step (see flint_netif.c).
 */
#ifndef FLINT_NETIF_H
#define FLINT_NETIF_H

#include "lwip/netif.h"
#include "lwip/err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pass to netif_add() as the init callback. */
err_t flint_netif_init(struct netif *netif);

/* Poll for received frames and push them into lwIP (call from the main loop or
   a task; a GENET RX interrupt will drive this once the driver lands). */
void  flint_netif_poll(struct netif *netif);

#ifdef __cplusplus
}
#endif

#endif /* FLINT_NETIF_H */
