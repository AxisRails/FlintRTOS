/*
 * FlintRTOS - host harness for the MQTT application (port/mqtt/flint_mqtt.c).
 *
 * Supplies a POSIX-socket implementation of mqtt_transport.h and a host
 * implementation of mqtt_platform.h, then runs the unmodified device app
 * against a real broker:
 *
 *   ./mqtt_host_test <host> <port> <client_id> <seconds>
 *
 * tests/mqtt_host_test.sh drives it against a local mosquitto, sends commands
 * with mosquitto_pub, kills it to fire the Last Will, and checks the topics.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "flint_mqtt.h"
#include "mqtt_transport.h"
#include "mqtt_platform.h"
#include "jsonw.h"

/* ---- transport (POSIX, non-blocking) -------------------------------------- */
struct NetworkContext { int fd; const char *err; };
static NetworkContext_t s_ctx = { -1, NULL };

NetworkContext_t *mqtt_transport_ctx(void) { return &s_ctx; }

int mqtt_transport_open(NetworkContext_t *c, const char *host, uint16_t port, uint32_t timeout_ms)
{
    struct addrinfo hints, *res = NULL;
    char ps[8];
    int fd, one = 1;
    struct timeval tv;
    (void)timeout_ms;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    snprintf(ps, sizeof(ps), "%u", port);
    if (getaddrinfo(host, ps, &hints, &res) != 0) { c->err = "DNS: host not found"; return -1; }
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if ((fd < 0) || (connect(fd, res->ai_addr, res->ai_addrlen) != 0))
    {
        c->err = "connection refused"; freeaddrinfo(res); if (fd >= 0) { close(fd); } return -1;
    }
    freeaddrinfo(res);
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    tv.tv_sec = 0; tv.tv_usec = 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));   /* ~= pump(1 ms) */
    c->fd = fd; c->err = NULL;
    return 0;
}

void mqtt_transport_close(NetworkContext_t *c) { if (c->fd >= 0) { close(c->fd); } c->fd = -1; }
bool mqtt_transport_is_up(const NetworkContext_t *c) { return c->fd >= 0; }
const char *mqtt_transport_error(const NetworkContext_t *c) { return c->err ? c->err : "none"; }

int32_t mqtt_transport_send(NetworkContext_t *c, const void *buf, size_t len)
{
    ssize_t n = send(c->fd, buf, len, MSG_NOSIGNAL);
    if (n < 0) { if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) { return 0; } c->err = "send"; return -1; }
    return (int32_t)n;
}

int32_t mqtt_transport_recv(NetworkContext_t *c, void *buf, size_t len)
{
    ssize_t n = recv(c->fd, buf, len, 0);
    if (n > 0) { return (int32_t)n; }
    if (n == 0) { c->err = "closed by broker"; close(c->fd); c->fd = -1; return -1; }
    if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) { return 0; }
    c->err = "recv"; return -1;
}

/* ---- platform ------------------------------------------------------------- */
static int s_led = 0;

uint32_t fm_plat_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((ts.tv_sec * 1000) + (ts.tv_nsec / 1000000));
}
void fm_plat_pump(bool idle) { if (idle) { struct timespec d = { 0, 1000000 }; nanosleep(&d, NULL); } }
const char *fm_plat_ip(void) { return "127.0.0.1"; }
int  fm_plat_led(int on) { s_led = (on < 0) ? !s_led : (on != 0); return s_led; }
void fm_plat_log(const char *line) { printf("%s\n", line); fflush(stdout); }

size_t fm_plat_ptp_json(char *buf, size_t cap)
{
    JsonW w;
    jw_init(&w, buf, cap);
    jw_obj_open(&w);
    jw_str(&w, "state", "LOCKED");
    jw_i64(&w, "offset_ns", -1234);
    jw_i64(&w, "delay_ns", 48000);
    jw_i64(&w, "freq_ppb", -37000);
    jw_obj_close(&w);
    return jw_len(&w);
}

size_t fm_plat_stats_json(char *buf, size_t cap)
{
    JsonW w;
    jw_init(&w, buf, cap);
    jw_obj_open(&w);
    jw_u32(&w, "uptime_s", 1);
    jw_arr_open(&w, "tasks");
    jw_obj_open(&w); jw_str(&w, "name", "net"); jw_u32(&w, "prio", 3); jw_obj_close(&w);
    jw_obj_open(&w); jw_str(&w, "name", "hb");  jw_u32(&w, "prio", 1); jw_obj_close(&w);
    jw_arr_close(&w);
    jw_obj_close(&w);
    return jw_len(&w);
}

int main(int argc, char **argv)
{
    const char *host = (argc > 1) ? argv[1] : "127.0.0.1";
    uint16_t    port = (uint16_t)((argc > 2) ? atoi(argv[2]) : 1883);
    const char *id   = (argc > 3) ? argv[3] : "flint-hosttest";
    int         secs = (argc > 4) ? atoi(argv[4]) : 10;
    uint32_t    t0;

    flint_mqtt_init(id, host, port);
    t0 = fm_plat_ms();
    while ((int)((fm_plat_ms() - t0) / 1000U) < secs)
    {
        flint_mqtt_service();
        fm_plat_pump(true);
    }
    printf("[host] connects=%u failures=%u published=%u commands=%u\n",
           flint_mqtt_stats()->connects, flint_mqtt_stats()->connect_failures,
           flint_mqtt_stats()->published, flint_mqtt_stats()->commands);
    fflush(stdout);
    /* Exit WITHOUT an MQTT DISCONNECT so the broker fires the Last Will. */
    _exit(0);
}
