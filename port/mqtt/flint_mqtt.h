/*
 * FlintRTOS - MQTT device application (coreMQTT).
 *
 * Topics, with <id> = the board's client ID (e.g. flint-10000000a1b2c3d4):
 *   flint/<id>/status  retained  {"state":"online",...}; Last Will = offline
 *   flint/<id>/ptp     1 Hz      PTP lock state, offset, delay, frequency
 *   flint/<id>/stats   10 s      uptime, heap, per-task stack high-water
 *   flint/<id>/cmd     subscribe ping | uptime | stats | ptp | led on|off|toggle | help
 *   flint/<id>/reply             answer to each command
 *
 * Runs entirely in the caller's context: call flint_mqtt_service() from the
 * network loop. It (re)connects with exponential back-off, drives
 * MQTT_ProcessLoop (keep-alive pings, incoming commands) and publishes on
 * schedule. Transport and platform are abstract (mqtt_transport.h,
 * mqtt_platform.h), so this same file is exercised on the host against a
 * real broker (tests/mqtt_host_test.c).
 */
#ifndef FLINT_MQTT_H
#define FLINT_MQTT_H

#include <stdint.h>
#include <stdbool.h>

void flint_mqtt_init(const char *client_id, const char *broker_host, uint16_t broker_port);
void flint_mqtt_service(void);
bool flint_mqtt_connected(void);

/* Counters for diagnostics / tests. */
typedef struct
{
    uint32_t connects;
    uint32_t connect_failures;
    uint32_t published;
    uint32_t commands;
} FlintMqttStats;
const FlintMqttStats *flint_mqtt_stats(void);

/* Build a topic "flint/<id>/<leaf>" (for tests). */
const char *flint_mqtt_topic(const char *leaf);

#endif /* FLINT_MQTT_H */
