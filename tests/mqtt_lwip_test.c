/*
 * FlintRTOS - end-to-end test of the Pi's MQTT stack on a Linux host:
 *   flint_mqtt.c  +  transport_raw.c  +  coreMQTT  +  lwIP (NO_SYS, raw API)
 * with lwIP's Ethernet frames carried over a Linux TAP interface instead of
 * GENET. Only the bottom (frame I/O) and the platform hooks differ from the
 * firmware; the transport and application are the exact files the Pi runs.
 *
 *   sudo ./mqtt_lwip_test <seconds>
 * Host side of the TAP: 10.77.0.1/24 (the broker listens there).
 * lwIP side:            10.77.0.2/24 (static, no DHCP server on the TAP).
 * Driven by tests/mqtt_host_test.sh (pass "lwip" as the mode).
 */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if_tun.h>

#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "lwip/etharp.h"
#include "lwip/ip4_addr.h"
#include "netif/ethernet.h"

#include "flint_mqtt.h"
#include "mqtt_transport.h"
#include "mqtt_platform.h"
#include "jsonw.h"

static int          s_tap = -1;
static struct netif s_nif;
static int          s_led;

u32_t sys_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u32_t)((ts.tv_sec * 1000) + (ts.tv_nsec / 1000000));
}

/* ---- TAP <-> lwIP --------------------------------------------------------- */
static int tap_open(const char *name)
{
    struct ifreq ifr;
    int fd = open("/dev/net/tun", O_RDWR | O_NONBLOCK);
    int s;
    struct sockaddr_in *sin;

    if (fd < 0) { perror("open /dev/net/tun"); return -1; }
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    strncpy(ifr.ifr_name, name, IFNAMSIZ - 1);
    if (ioctl(fd, TUNSETIFF, &ifr) < 0) { perror("TUNSETIFF"); close(fd); return -1; }

    s = socket(AF_INET, SOCK_DGRAM, 0);
    sin = (struct sockaddr_in *)&ifr.ifr_addr;
    sin->sin_family = AF_INET;
    inet_pton(AF_INET, "10.77.0.1", &sin->sin_addr);
    if (ioctl(s, SIOCSIFADDR, &ifr) < 0) { perror("SIOCSIFADDR"); }
    inet_pton(AF_INET, "255.255.255.0", &sin->sin_addr);
    if (ioctl(s, SIOCSIFNETMASK, &ifr) < 0) { perror("SIOCSIFNETMASK"); }
    if (ioctl(s, SIOCGIFFLAGS, &ifr) == 0)
    {
        ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
        if (ioctl(s, SIOCSIFFLAGS, &ifr) < 0) { perror("SIOCSIFFLAGS"); }
    }
    close(s);
    return fd;
}

static err_t tap_output(struct netif *n, struct pbuf *p)
{
    uint8_t frame[1600];
    (void)n;
    if (p->tot_len > sizeof(frame)) { return ERR_BUF; }
    pbuf_copy_partial(p, frame, p->tot_len, 0);
    return (write(s_tap, frame, p->tot_len) == (ssize_t)p->tot_len) ? ERR_OK : ERR_IF;
}

static err_t tap_netif_init(struct netif *n)
{
    static const uint8_t mac[6] = { 0x02, 0x00, 0x5E, 0x00, 0x00, 0x42 };
    n->name[0] = 't'; n->name[1] = 'p';
    n->output = etharp_output;
    n->linkoutput = tap_output;
    n->mtu = 1500;
    n->hwaddr_len = 6;
    memcpy(n->hwaddr, mac, 6);
    n->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET | NETIF_FLAG_LINK_UP;
    return ERR_OK;
}

static void tap_poll(void)
{
    uint8_t frame[1600];
    for (;;)
    {
        ssize_t n = read(s_tap, frame, sizeof(frame));
        if (n <= 0) { break; }
        struct pbuf *p = pbuf_alloc(PBUF_RAW, (u16_t)n, PBUF_POOL);
        if (p == NULL) { break; }
        pbuf_take(p, frame, (u16_t)n);
        if (s_nif.input(p, &s_nif) != ERR_OK) { pbuf_free(p); }
    }
}

/* ---- mqtt_platform.h ------------------------------------------------------ */
uint32_t fm_plat_ms(void) { return sys_now(); }
void fm_plat_pump(bool idle)
{
    tap_poll();
    sys_check_timeouts();
    if (idle) { usleep(1000); }
}
const char *fm_plat_ip(void) { return ip4addr_ntoa(netif_ip4_addr(&s_nif)); }
int  fm_plat_led(int on) { s_led = (on < 0) ? !s_led : (on != 0); return s_led; }
void fm_plat_log(const char *line) { printf("%s\n", line); fflush(stdout); }
size_t fm_plat_ptp_json(char *buf, size_t cap)
{
    JsonW w;
    jw_init(&w, buf, cap);
    jw_obj_open(&w); jw_str(&w, "state", "LOCKED"); jw_i64(&w, "offset_ns", -1234); jw_obj_close(&w);
    return jw_len(&w);
}
size_t fm_plat_stats_json(char *buf, size_t cap)
{
    JsonW w;
    jw_init(&w, buf, cap);
    jw_obj_open(&w); jw_u32(&w, "uptime_s", fm_plat_ms() / 1000U); jw_obj_close(&w);
    return jw_len(&w);
}

int main(int argc, char **argv)
{
    int      secs = (argc > 1) ? atoi(argv[1]) : 10;
    uint16_t port = (uint16_t)((argc > 2) ? atoi(argv[2]) : 1883);
    const char *id = (argc > 3) ? argv[3] : "flint-lwiptest";
    ip4_addr_t ip, mask, gw;
    uint32_t t0;

    s_tap = tap_open("flint0");
    if (s_tap < 0) { return 2; }

    lwip_init();
    IP4_ADDR(&ip, 10, 77, 0, 2); IP4_ADDR(&mask, 255, 255, 255, 0); IP4_ADDR(&gw, 10, 77, 0, 1);
    netif_add(&s_nif, &ip, &mask, &gw, NULL, tap_netif_init, ethernet_input);
    netif_set_default(&s_nif);
    netif_set_up(&s_nif);
    printf("[lwip] host lwIP %s up on TAP flint0\n", ip4addr_ntoa(&ip));

    flint_mqtt_init(id, "10.77.0.1", port);
    t0 = sys_now();
    while ((sys_now() - t0) < (uint32_t)secs * 1000U)
    {
        fm_plat_pump(false);
        flint_mqtt_service();
        usleep(1000);
    }
    printf("[host] connects=%u failures=%u published=%u commands=%u\n",
           flint_mqtt_stats()->connects, flint_mqtt_stats()->connect_failures,
           flint_mqtt_stats()->published, flint_mqtt_stats()->commands);
    fflush(stdout);
    /* Drop TCP without an MQTT DISCONNECT (like a cable pull the broker can
       see): the broker must publish the Last Will. */
    mqtt_transport_close(mqtt_transport_ctx());
    for (int i = 0; i < 200; i++) { fm_plat_pump(true); }
    _exit(0);
}
