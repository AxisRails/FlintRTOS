/*
 * FlintRTOS - lwIP port configuration (lwipopts.h).
 *
 * Two modes, selected by the build:
 *   - default (NO_SYS=1): raw/callback API, no OS primitives. `make`.
 *   - OS mode  (NO_SYS=0): sequential (netconn) + BSD sockets, backed by the
 *     FlintRTOS sys_arch (sem/mbox/thread). `make LWIP_OS=1`. Required by
 *     coreMQTT/coreHTTP.
 * IPv4 + IPv6, UDP + TCP, ARP/Ethernet, ICMP, DHCP in both modes.
 */
#ifndef FLINT_LWIPOPTS_H
#define FLINT_LWIPOPTS_H

/* ---- OS mode selection --------------------------------------------------- */
#ifdef FLINT_LWIP_OS
#define NO_SYS                      0
#define LWIP_NETCONN                1
#define LWIP_SOCKET                 1
#define LWIP_TCPIP_CORE_LOCKING     1
#define TCPIP_THREAD_STACKSIZE      4096
#define TCPIP_THREAD_PRIO           8
#define TCPIP_MBOX_SIZE             16
#define DEFAULT_UDP_RECVMBOX_SIZE   16
#define DEFAULT_TCP_RECVMBOX_SIZE   16
#define DEFAULT_ACCEPTMBOX_SIZE     8
#define DEFAULT_THREAD_STACKSIZE    2048
#define LWIP_NETIF_API              1
#define LWIP_SO_RCVTIMEO            1
#define LWIP_SO_SNDTIMEO            1
#else
#define NO_SYS                      1
#define LWIP_NETCONN                0
#define LWIP_SOCKET                 0
#define LWIP_NETIF_API              0
#endif

#define SYS_LIGHTWEIGHT_PROT        1
#define LWIP_ERRNO_INCLUDE          <errno.h>

/* ---- Memory -------------------------------------------------------------- */
#define MEM_LIBC_MALLOC             0
#define MEMP_MEM_MALLOC             0
#define MEM_ALIGNMENT               8U
#define MEM_SIZE                    (128 * 1024)
#define MEMP_NUM_PBUF               32
#define MEMP_NUM_UDP_PCB            8
#define MEMP_NUM_TCP_PCB            8
#define MEMP_NUM_TCP_PCB_LISTEN     4
#define MEMP_NUM_TCP_SEG            32
#define MEMP_NUM_NETCONN            16
#define PBUF_POOL_SIZE              32

/* ---- Protocols ----------------------------------------------------------- */
#define LWIP_IPV4                   1
#define LWIP_IPV6                   1
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_ICMP6                  1
#define LWIP_RAW                    1
#define LWIP_UDP                    1
#define LWIP_TCP                    1
#define LWIP_DHCP                   1
#define LWIP_AUTOIP                 0
#define LWIP_ACD                    1
#define LWIP_IGMP                   0
#define LWIP_IPV6_MLD               0
#define LWIP_DNS                    1

/* ---- Netif --------------------------------------------------------------- */
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_SINGLE_NETIF           1

/* ---- Checksums ----------------------------------------------------------- */
#define CHECKSUM_GEN_IP             1
#define CHECKSUM_GEN_UDP            1
#define CHECKSUM_GEN_TCP            1
#define CHECKSUM_CHECK_IP           1
#define CHECKSUM_CHECK_UDP          1
#define CHECKSUM_CHECK_TCP          1

/* ---- Footprint ----------------------------------------------------------- */
#define LWIP_STATS                  0
#define LWIP_DEBUG                  0
#define LWIP_NETCONN_SEM_PER_THREAD 0

#endif /* FLINT_LWIPOPTS_H */
