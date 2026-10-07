/*
 * FlintRTOS - lwIP network demo (RPi4), NO_SYS mode. Enabled by configUSE_LWIP.
 *
 * One task owns the whole network stack. net_pump() is one iteration of it:
 * drain GENET RX into lwIP, run lwIP timers (DHCP, ARP, TCP, DNS, IGMP), run
 * the PTP slave, and watch link/DHCP. The task loop calls net_pump() and then
 * the MQTT client; whenever the MQTT transport has to wait (DNS, TCP connect,
 * CONNACK, a full send window) it calls net_pump() itself through
 * fm_plat_pump(), so the stack never stalls behind a blocking MQTT call.
 *
 * Services: DHCP client, UDP echo on :7, IEEE 1588 slave (configUSE_PTP),
 * MQTT device client (configUSE_CORE_MQTT) - see port/mqtt/flint_mqtt.h.
 */
#include "FlintRTOS.h"

#if (configUSE_LWIP == 1)

#include "task.h"
#include "uart.h"

#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "lwip/udp.h"
#include "lwip/etharp.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include "lwip/ip4_addr.h"
#include "flint_netif.h"
#include "genet.h"
#include "net_demo.h"
#if (configUSE_PTP == 1)
#include "ptp_lwip_raw.h"
#endif
#if (configUSE_CORE_MQTT == 1)
#include "flint_mqtt.h"
#include "mailbox.h"
#include "jsonw.h"
#endif

static struct netif s_netif;
static bool     s_bound;
static bool     s_link_up;
static uint32_t s_last_link_ms;
static uint32_t s_last_report_ms;

struct netif *net_demo_netif(void) { return &s_netif; }
bool          net_demo_bound(void) { return s_bound; }

static void udp_echo_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                          const ip_addr_t *addr, u16_t port)
{
    (void)arg;
    if (p != NULL)
    {
        uart_printf("[net] UDP rx %u bytes on :7 -> echoing\n", (unsigned int)p->tot_len);
        (void)udp_sendto(pcb, p, addr, port);
        pbuf_free(p);
    }
}

static void flint_net_init(void)
{
    ip4_addr_t ipaddr, netmask, gw;

    lwip_init();

    /* Start with 0.0.0.0 - the DHCP client fills these in from the server. */
    IP4_ADDR(&ipaddr,  0, 0, 0, 0);
    IP4_ADDR(&netmask, 0, 0, 0, 0);
    IP4_ADDR(&gw,      0, 0, 0, 0);

    (void)netif_add(&s_netif, &ipaddr, &netmask, &gw, NULL,
                    flint_netif_init, netif_input);   /* calls genet_init() */
    netif_set_default(&s_netif);
    netif_set_up(&s_netif);
    (void)dhcp_start(&s_netif);                       /* request a lease */

    {
        struct udp_pcb *pcb = udp_new();
        if (pcb != NULL)
        {
            (void)udp_bind(pcb, IP_ANY_TYPE, 7);
            udp_recv(pcb, udp_echo_recv, NULL);
        }
    }

    uart_printf("[net] lwIP %d.%d.%d up; DHCP started; UDP echo on :7\n",
                LWIP_VERSION_MAJOR, LWIP_VERSION_MINOR, LWIP_VERSION_REVISION);
}

/* Use the DHCP-provided DNS servers; fall back to public resolvers. */
static void dns_setup(void)
{
    const ip_addr_t *d0 = dns_getserver(0);
    if ((d0 == NULL) || ip_addr_isany(d0))
    {
        ip_addr_t a, b;
        IP_ADDR4(&a, 1, 1, 1, 1);
        IP_ADDR4(&b, 8, 8, 8, 8);
        dns_setserver(0, &a);
        dns_setserver(1, &b);
        uart_printf("[net]   DNS = 1.1.1.1, 8.8.8.8 (none from DHCP)\n");
    }
    else
    {
        uart_printf("[net]   DNS = %s (from DHCP)\n", ipaddr_ntoa(d0));
    }
}

#if (configUSE_CORE_MQTT == 1)
/* Client ID from the board serial (unique per Pi), else from the MAC. */
static void mqtt_start(void)
{
    char     id[32];
    char     hex[17];
    uint64_t serial = 0U;

    fm_strlcpy(id, "flint-", sizeof(id));
    if (mbox_get_serial(&serial))
    {
        fm_strlcat(id, fm_hex64(serial, hex, 16U), sizeof(id));
    }
    else
    {
        uint64_t m = 0U;
        for (uint32_t i = 0U; i < 6U; i++) { m = (m << 8) | s_netif.hwaddr[i]; }
        fm_strlcat(id, fm_hex64(m, hex, 12U), sizeof(id));
    }
    flint_mqtt_init(id, configMQTT_BROKER_HOST, (uint16_t)configMQTT_BROKER_PORT);
    uart_printf("[mqtt] broker %s:%u  ->  mosquitto_sub -h %s -t 'flint/%s/#' -v\n",
                configMQTT_BROKER_HOST, (unsigned int)configMQTT_BROKER_PORT,
                configMQTT_BROKER_HOST, id);
}
#endif

void net_pump(bool idle)
{
    uint32_t now;

    flint_netif_poll(&s_netif);   /* drain RX -> lwIP (callbacks run here) */
    sys_check_timeouts();         /* DHCP, ARP, TCP, DNS, IGMP timers      */
    now = (uint32_t)xTaskGetTickCount();

#if (configUSE_PTP == 1)
    ptp_raw_poll(now);
#endif

    /* Announce a DHCP lease the moment it binds, then start the services. */
    if (!s_bound && (ip4_addr_get_u32(netif_ip4_addr(&s_netif)) != 0U))
    {
        s_bound = true;
        uart_printf("\n*** [net] DHCP LEASE ACQUIRED ***\n");
        uart_printf("[net]   IP  = %s\n", ip4addr_ntoa(netif_ip4_addr(&s_netif)));
        uart_printf("[net]   GW  = %s\n", ip4addr_ntoa(netif_ip4_gw(&s_netif)));
        uart_printf("[net]   MASK= %s\n", ip4addr_ntoa(netif_ip4_netmask(&s_netif)));
        dns_setup();
        uart_printf("[net]   ping me, or send UDP to :7\n\n");
#if (configUSE_PTP == 1)
        (void)ptp_raw_start(&s_netif);        /* IEEE 1588 slave on 319/320 */
#endif
#if (configUSE_CORE_MQTT == 1)
        mqtt_start();
#endif
    }

    /* Link check once a second; ring/DHCP report every 30 s. */
    if ((uint32_t)(now - s_last_link_ms) >= 1000U)
    {
        bool up = genet_link_up();
        s_last_link_ms = now;
        if (up != s_link_up)
        {
            uart_printf("[net] LINK %s\n", up ? "UP" : "DOWN");
            s_link_up = up;
            if (up) { genet_adjust_link(); }
        }
    }
    if ((uint32_t)(now - s_last_report_ms) >= 30000U)
    {
        const struct dhcp *d = netif_dhcp_data(&s_netif);
        s_last_report_ms = now;
        genet_diag_rings();
        uart_printf("[net] IP = %s  | DHCP state=%u\n",
                    ip4addr_ntoa(netif_ip4_addr(&s_netif)),
                    (d != NULL) ? (unsigned int)d->state : 0U);
    }

    if (idle) { vTaskDelay(1U); }
}

void vNetworkTask(void *pvParameters)
{
    (void)pvParameters;

    uart_printf("\n[net] ===== GENET / lwIP bring-up =====\n");
    flint_net_init();
    genet_diag();
    genet_probe();
    s_last_report_ms = (uint32_t)xTaskGetTickCount();

    for (;;)
    {
        net_pump(false);
#if (configUSE_CORE_MQTT == 1)
        if (s_bound) { flint_mqtt_service(); }
#endif
        vTaskDelay(1U);
    }
}

#endif /* configUSE_LWIP */
