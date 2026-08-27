/*
 * FlintRTOS - coreMQTT transport over lwIP BSD sockets.
 * Plain TCP (no TLS yet - a mbedTLS layer slots in here for coreMQTT-over-TLS).
 */
#include "transport_lwip.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"

int transport_connect(NetworkContext_t *ctx, const char *host, uint16_t port)
{
    struct sockaddr_in addr;
    int s = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
    {
        return -1;
    }

    /* Small receive timeout so transport_recv() polls without blocking MQTT. */
    {
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 20000;   /* 20 ms */
        (void)lwip_setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    addr.sin_family = AF_INET;
    addr.sin_port   = lwip_htons(port);
    addr.sin_addr.s_addr = ipaddr_addr(host);   /* dotted-quad host */

    if (lwip_connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        lwip_close(s);
        return -2;
    }

    ctx->socket = s;
    return 0;
}

void transport_disconnect(NetworkContext_t *ctx)
{
    if (ctx->socket >= 0)
    {
        (void)lwip_close(ctx->socket);
        ctx->socket = -1;
    }
}

int32_t transport_send(NetworkContext_t *ctx, const void *buf, size_t len)
{
    int n = lwip_send(ctx->socket, buf, len, 0);
    return (int32_t)n;
}

int32_t transport_recv(NetworkContext_t *ctx, void *buf, size_t len)
{
    int n = lwip_recv(ctx->socket, buf, len, 0);
    if (n < 0)
    {
        /* Receive timeout / would-block -> no data available (not an error). */
        if ((errno == EWOULDBLOCK) || (errno == EAGAIN) || (errno == 0))
        {
            return 0;
        }
        return -1;
    }
    return (int32_t)n;
}
