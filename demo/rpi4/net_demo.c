/*
 * FlintRTOS - lwIP network demo (RPi4). Enabled by configUSE_LWIP.
 *
 * NO_SYS mode: a dedicated FlintRTOS task brings lwIP up on the Ethernet netif,
 * opens a UDP echo server on port 7, then pumps the stack (poll RX + service
 * timeouts). Frames actually move once the GENET MAC driver fills in
 * flint_low_level_output/flint_netif_poll; until then the stack initialises and
 * runs (verifiable on hardware/QEMU).
 */
#include "FlintRTOS.h"

#if (configUSE_LWIP == 1)

#include "task.h"
#include "uart.h"

#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "flint_netif.h"

static struct netif s_netif;

static void udp_echo_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                          const ip_addr_t *addr, u16_t port)
{
    (void)arg;
    if (p != NULL)
    {
        (void)udp_sendto(pcb, p, addr, port);   /* echo it back */
        pbuf_free(p);
    }
}

static void flint_net_init(void)
{
    ip4_addr_t ipaddr, netmask, gw;

    lwip_init();

    IP4_ADDR(&ipaddr,  192, 168, 1, 50);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw,      192, 168, 1, 1);

    (void)netif_add(&s_netif, &ipaddr, &netmask, &gw, NULL,
                    flint_netif_init, netif_input);
    netif_set_default(&s_netif);
    netif_set_up(&s_netif);

    {
        struct udp_pcb *pcb = udp_new();
        if (pcb != NULL)
        {
            (void)udp_bind(pcb, IP_ANY_TYPE, 7);
            udp_recv(pcb, udp_echo_recv, NULL);
        }
    }

    uart_printf("[net] lwIP %d.%d.%d up @ 192.168.1.50; UDP echo on :7\n",
                LWIP_VERSION_MAJOR, LWIP_VERSION_MINOR, LWIP_VERSION_REVISION);
}

void vNetworkTask(void *pvParameters)
{
    (void)pvParameters;
    flint_net_init();
    for (;;)
    {
        flint_netif_poll(&s_netif);   /* drain RX -> lwIP (GENET driver TODO) */
        sys_check_timeouts();         /* lwIP periodic processing            */
        vTaskDelay(1U);
    }
}

#endif /* configUSE_LWIP */
