/*
 * FlintRTOS - PTP ordinary-clock SLAVE (IEEE 1588v2 over UDP, lwIP OS mode).
 * Synchronises the local disciplined clock to a PTP master via
 * Sync/Follow_Up/Delay_Req/Delay_Resp. Simplified BMC: tracks the first master
 * seen. Requires lwIP OS mode (sockets).
 */
#ifndef FLINT_PTP_SLAVE_H
#define FLINT_PTP_SLAVE_H

#include <stdint.h>

/* Run the PTP slave forever (intended as a FlintRTOS task body).
   mac[6] seeds the local clock identity. */
void ptp_slave_run(const uint8_t mac[6]);

/* Task entry (pvParameters ignored). */
void vPtpTask(void *pvParameters);

#endif /* FLINT_PTP_SLAVE_H */
