/*
 * FlintRTOS - coreMQTT transport over lwIP BSD sockets.
 * Defines NetworkContext (a socket) and the send/recv the TransportInterface_t
 * needs, plus a TCP connect helper. Requires lwIP OS mode (LWIP_SOCKET=1).
 */
#ifndef FLINT_TRANSPORT_LWIP_H
#define FLINT_TRANSPORT_LWIP_H

#include <stdint.h>
#include <stddef.h>

/* coreMQTT references this incomplete type by name; we complete it here. */
struct NetworkContext
{
    int socket;
};
typedef struct NetworkContext NetworkContext_t;

/* Connect a TCP socket to host:port. Returns 0 on success, negative on error. */
int      transport_connect(NetworkContext_t *ctx, const char *host, uint16_t port);
void     transport_disconnect(NetworkContext_t *ctx);

/* TransportInterface_t callbacks. */
int32_t  transport_send(NetworkContext_t *ctx, const void *buf, size_t len);
int32_t  transport_recv(NetworkContext_t *ctx, void *buf, size_t len);

#endif /* FLINT_TRANSPORT_LWIP_H */
