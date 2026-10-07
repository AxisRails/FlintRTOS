/*
 * FlintRTOS - coreMQTT configuration. Logging disabled (route to UART later).
 * Remaining parameters use coreMQTT's documented defaults.
 */
#ifndef CORE_MQTT_CONFIG_H
#define CORE_MQTT_CONFIG_H

#define LogError(message)
#define LogWarn(message)
#define LogInfo(message)
#define LogDebug(message)

/* The raw-TCP transport returns 0 while it pumps lwIP for more bytes; give a
   packet split across TCP segments time to complete. */
#define MQTT_RECV_POLLING_TIMEOUT_MS  (1000U)

/* Keep the state arrays modest for an embedded target. */
#define MQTT_STATE_ARRAY_MAX_COUNT   (10U)

#endif /* CORE_MQTT_CONFIG_H */
