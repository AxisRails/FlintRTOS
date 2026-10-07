/*
 * FlintRTOS - NO_SYS network task (demo/rpi4/net_demo.c).
 */
#ifndef FLINT_NET_DEMO_H
#define FLINT_NET_DEMO_H

#include <stdbool.h>
#include "lwip/netif.h"

/* One iteration of the network stack; idle=true also sleeps 1 tick. Must only
   be called from the network task (lwIP NO_SYS is single-threaded). */
void          net_pump(bool idle);
struct netif *net_demo_netif(void);
bool          net_demo_bound(void);

#endif /* FLINT_NET_DEMO_H */
