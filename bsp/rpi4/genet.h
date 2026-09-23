/*
 * FlintRTOS - BCM2711 GENET v5 Ethernet MAC driver (Raspberry Pi 4).
 *
 * Public interface used by the lwIP netif (port/lwip/netif/flint_netif.c).
 *
 * STATUS: build-verified, hardware bring-up pending. The register map and DMA
 * ring logic follow the published BCM2711 GENET v5 layout (as used by the Linux
 * bcmgenet driver and bare-metal ports). On real hardware, first validate MDIO
 * PHY access and the RX/TX descriptor rings.
 */
#ifndef FLINT_BSP_GENET_H
#define FLINT_BSP_GENET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define GENET_MAX_FRAME   (1536U)   /* buffer size per descriptor */

/* Bring the MAC up: reset, MDIO/PHY, MAC address, DMA rings, enable RX/TX.
   Returns true on success. mac[6] is the station address to program. */
bool genet_init(const uint8_t mac[6]);

/* Transmit one contiguous frame (len bytes). Returns true if queued. */
bool genet_send(const uint8_t *frame, uint16_t len);

/*
 * Poll for one received frame. If a frame is available, copies up to *len bytes
 * into buf, sets *len to the frame length, and returns true. Returns false if
 * no frame is pending. (An interrupt-driven path can replace polling later.)
 */
bool genet_recv(uint8_t *buf, uint16_t *len);

/* True once the PHY reports link up. */
bool genet_link_up(void);

/* Bring-up diagnostics (print over UART): GENET revision, MDIO PHY discovery,
 * link/autoneg state, and DMA ring producer/consumer indices. */
void genet_diag(void);
void genet_diag_rings(void);
void genet_probe(void);

#endif /* FLINT_BSP_GENET_H */
