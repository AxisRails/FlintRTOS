/*
 * FlintRTOS - coreMQTT transport interface (transport-neutral).
 *
 * The MQTT application (flint_mqtt.c) talks to the network only through this
 * API, so the same application code runs on the Pi (transport_raw.c: lwIP raw
 * TCP, NO_SYS) and on a development host (tests/mqtt_host_transport.c: POSIX
 * sockets) for testing against a real broker. NetworkContext_t stays opaque.
 */
#ifndef FLINT_MQTT_TRANSPORT_H
#define FLINT_MQTT_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

struct NetworkContext;
typedef struct NetworkContext NetworkContext_t;

/* The (single) connection context. */
NetworkContext_t *mqtt_transport_ctx(void);

/* Resolve host (DNS name or dotted quad) and open TCP to host:port.
   Blocks up to timeout_ms while keeping the network stack running. */
int     mqtt_transport_open(NetworkContext_t *ctx, const char *host, uint16_t port,
                            uint32_t timeout_ms);
void    mqtt_transport_close(NetworkContext_t *ctx);
bool    mqtt_transport_is_up(const NetworkContext_t *ctx);

/* coreMQTT TransportInterface_t callbacks: bytes moved, 0 = try again,
   negative = connection lost. */
int32_t mqtt_transport_send(NetworkContext_t *ctx, const void *buf, size_t len);
int32_t mqtt_transport_recv(NetworkContext_t *ctx, void *buf, size_t len);

/* Text of the last failure (for logs). */
const char *mqtt_transport_error(const NetworkContext_t *ctx);

#endif /* FLINT_MQTT_TRANSPORT_H */
