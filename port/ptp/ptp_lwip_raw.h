/*
 * FlintRTOS - PTP slave bound to lwIP's raw UDP API (NO_SYS mode).
 *
 * Opens UDP 319 (event) and 320 (general), joins 224.0.1.129 via IGMP, and
 * feeds ptp_core with the GENET driver's interrupt-time RX timestamps and
 * doorbell-time TX timestamps. Everything runs in the network task's context
 * (the same one that calls flint_netif_poll / sys_check_timeouts).
 */
#ifndef FLINT_PTP_LWIP_RAW_H
#define FLINT_PTP_LWIP_RAW_H

#include <stdbool.h>
#include "lwip/netif.h"

/* Start the slave on nif (call once it has an IPv4 address). */
bool ptp_raw_start(struct netif *nif);

/* Call from the network loop (every ms or so). */
void ptp_raw_poll(uint32_t now_ms);

/* True once ptp_raw_start() succeeded. */
bool ptp_raw_running(void);

#endif /* FLINT_PTP_LWIP_RAW_H */
