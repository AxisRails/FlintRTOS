/*
 * FlintRTOS - VideoCore property mailbox (BCM2711, channel 8).
 * Used to read the board's identity from the GPU firmware: the factory MAC
 * address (so every Pi gets its own address on the LAN) and the board serial
 * (a stable unique ID for MQTT client IDs / topics).
 */
#ifndef FLINT_BSP_MAILBOX_H
#define FLINT_BSP_MAILBOX_H

#include <stdint.h>
#include <stdbool.h>

/* Each returns false (and leaves *out untouched) if the firmware didn't answer. */
bool mbox_get_mac(uint8_t mac[6]);
bool mbox_get_serial(uint64_t *serial);

#endif /* FLINT_BSP_MAILBOX_H */
