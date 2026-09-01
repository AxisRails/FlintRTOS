/*
 * FlintRTOS - lwIP network demo (RPi4), NO_SYS mode. Enabled by configUSE_LWIP.
 *
 * Bring-up focus: after initialising lwIP + the GENET MAC, print a diagnostic
 * report (GENET revision, MDIO PHY discovery, link/autoneg), then run the stack
 * with a UDP echo server on :7. Each cycle it reports link state, the DMA ring
 * indices, and (once link is up) periodically sends a gratuitous ARP to test
 * the TX path - so the console shows exactly how far the Ethernet path gets.
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
#include "lwip/ip_addr.h"
#include "flint_netif.h"
#include "genet.h"

static struct netif s_netif;

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

    IP4_ADDR(&ipaddr,  192, 168, 1, 50);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw,      192, 168, 1, 1);

    (void)netif_add(&s_netif, &ipaddr, &netmask, &gw, NULL,
                    flint_netif_init, netif_input);   /* calls genet_init() */
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
    bool     was_up = false;
    uint32_t report = 0U;

    uart_printf("\n[net] ===== GENET / lwIP bring-up =====\n");
    flint_net_init();

    /* Foundation check: is GENET powered/addressed and can MDIO reach the PHY? */
    genet_diag();

    for (;;)
    {
        flint_netif_poll(&s_netif);   /* drain RX -> lwIP */
        sys_check_timeouts();

        /* Roughly once a second (1 ms poll delay). */
        report++;
        if (report >= 1000U)
        {
            report = 0U;
            bool up = genet_link_up();
            if (up != was_up)
            {
                uart_printf("[net] LINK %s\n", up ? "UP" : "DOWN");
                was_up = up;
                if (up)
                {
                    etharp_gratuitous(&s_netif);   /* announce ourselves (TX test) */
                }
            }
            genet_diag_rings();
            if (up)
            {
                etharp_gratuitous(&s_netif);       /* periodic TX to watch cons idx */
            }
        }

        vTaskDelay(1U);
    }
}

#endif /* configUSE_LWIP */
