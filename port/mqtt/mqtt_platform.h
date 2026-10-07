/*
 * FlintRTOS - platform services the MQTT application needs.
 *
 * Implemented by demo/rpi4/mqtt_platform.c on the Pi and by
 * tests/mqtt_host_platform.c on a development host.
 */
#ifndef FLINT_MQTT_PLATFORM_H
#define FLINT_MQTT_PLATFORM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Monotonic milliseconds. */
uint32_t fm_plat_ms(void);

/* Run the network stack once (RX, timers, PTP...). idle=true: nothing else
   to do, so also give the CPU away for ~1 ms. Never calls back into MQTT. */
void     fm_plat_pump(bool idle);

/* Current IPv4 address as text. */
const char *fm_plat_ip(void);

/* Board activity LED: on = 1/0, or -1 to toggle. Returns the new state. */
int      fm_plat_led(int on);

/* Fill buf with a JSON object; return its length (0 = nothing to report). */
size_t   fm_plat_ptp_json(char *buf, size_t cap);
size_t   fm_plat_stats_json(char *buf, size_t cap);

/* Logging (one line, no trailing newline needed). */
void     fm_plat_log(const char *line);

#endif /* FLINT_MQTT_PLATFORM_H */
