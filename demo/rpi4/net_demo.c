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
#include "lwip/dhcp.h"
#include "lwip/ip_addr.h"
#include "lwip/ip4_addr.h"
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
    uart_printf("[net] MAC = %x:%x:%x:%x:%x:%x  (DHCP states: 6=SELECTING 1=REQUESTING 10=BOUND)\n",
                s_netif.hwaddr[0], s_netif.hwaddr[1], s_netif.hwaddr[2],
                s_netif.hwaddr[3], s_netif.hwaddr[4], s_netif.hwaddr[5]);
}

void vNetworkTask(void *pvParameters)
{
    (void)pvParameters;
    bool     was_up = false;
    bool     bound  = false;
    uint32_t report = 0U;

    uart_printf("\n[net] ===== GENET / lwIP bring-up =====\n");
    flint_net_init();

    /* Foundation check: is GENET powered/addressed and can MDIO reach the PHY? */
    genet_diag();
    genet_probe();   /* ground-truth DMA register layout + enable state */

    for (;;)
    {
        flint_netif_poll(&s_netif);   /* drain RX -> lwIP */
        sys_check_timeouts();         /* drives the DHCP state machine */

        /* Announce a DHCP lease the moment it binds. */
        if (!bound && (ip4_addr_get_u32(netif_ip4_addr(&s_netif)) != 0U))
        {
            bound = true;
            uart_printf("\n*** [net] DHCP LEASE ACQUIRED ***\n");
            uart_printf("[net]   IP  = %s\n", ip4addr_ntoa(netif_ip4_addr(&s_netif)));
            uart_printf("[net]   GW  = %s\n", ip4addr_ntoa(netif_ip4_gw(&s_netif)));
            uart_printf("[net]   MASK= %s\n", ip4addr_ntoa(netif_ip4_netmask(&s_netif)));
            uart_printf("[net]   ping me, or send UDP to :7\n\n");
        }

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
                    genet_adjust_link();   /* program MAC for negotiated speed */
                }
            }
            genet_diag_rings();
            {
                const struct dhcp *d = netif_dhcp_data(&s_netif);
                uart_printf("[net] current IP = %s  | DHCP state=%u tries=%u xid=0x%x\n",
                            ip4addr_ntoa(netif_ip4_addr(&s_netif)),
                            (d != NULL) ? (unsigned int)d->state : 0U,
                            (d != NULL) ? (unsigned int)d->tries : 0U,
                            (d != NULL) ? (unsigned int)d->xid : 0U);
            }
        }

        vTaskDelay(1U);
    }
}

#endif /* configUSE_LWIP */
