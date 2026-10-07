/*
 * Host lwIP options for tests/mqtt_lwip_test.c: the same NO_SYS raw-API
 * configuration as the Pi image (port/lwip/include/lwipopts.h) for the parts
 * the MQTT transport exercises - TCP window/MSS/queue, DNS, memory - minus
 * IPv6/IGMP/PTP, which the test doesn't need.
 */
#ifndef FLINT_TEST_LWIPOPTS_H
#define FLINT_TEST_LWIPOPTS_H
#define NO_SYS                      1
#define LWIP_NETCONN                0
#define LWIP_SOCKET                 0
#define SYS_LIGHTWEIGHT_PROT        0
#define MEM_ALIGNMENT               8U
#define MEM_SIZE                    (128 * 1024)
#define MEMP_NUM_PBUF               32
#define MEMP_NUM_TCP_PCB            8
#define MEMP_NUM_TCP_SEG            32
#define PBUF_POOL_SIZE              32
#define TCP_MSS                     1460
#define TCP_SND_BUF                 (4 * TCP_MSS)
#define TCP_WND                     (4 * TCP_MSS)
#define TCP_SND_QUEUELEN            ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))
#define LWIP_TCP_KEEPALIVE          1
#define LWIP_IPV4                   1
#define LWIP_IPV6                   0
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_UDP                    1
#define LWIP_TCP                    1
#define LWIP_DHCP                   0
#define LWIP_DNS                    1
#define LWIP_STATS                  0
#define CHECKSUM_GEN_IP             1
#define CHECKSUM_GEN_UDP            1
#define CHECKSUM_GEN_TCP            1
#define CHECKSUM_CHECK_IP           1
#define CHECKSUM_CHECK_UDP          1
#define CHECKSUM_CHECK_TCP          1
#endif
