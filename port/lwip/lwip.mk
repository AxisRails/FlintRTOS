# FlintRTOS - lwIP source list + include paths (included by the top Makefile
# when LWIP=1). lwIP 2.2.0, NO_SYS mode, IPv4 + IPv6.
LWIPDIR := third_party/lwip/src

LWIP_INC := -I$(LWIPDIR)/include -Iport/lwip/include

LWIP_CORE := \
    $(LWIPDIR)/core/init.c \
    $(LWIPDIR)/core/def.c \
    $(LWIPDIR)/core/dns.c \
    $(LWIPDIR)/core/inet_chksum.c \
    $(LWIPDIR)/core/ip.c \
    $(LWIPDIR)/core/mem.c \
    $(LWIPDIR)/core/memp.c \
    $(LWIPDIR)/core/netif.c \
    $(LWIPDIR)/core/pbuf.c \
    $(LWIPDIR)/core/raw.c \
    $(LWIPDIR)/core/stats.c \
    $(LWIPDIR)/core/sys.c \
    $(LWIPDIR)/core/altcp.c \
    $(LWIPDIR)/core/altcp_alloc.c \
    $(LWIPDIR)/core/altcp_tcp.c \
    $(LWIPDIR)/core/tcp.c \
    $(LWIPDIR)/core/tcp_in.c \
    $(LWIPDIR)/core/tcp_out.c \
    $(LWIPDIR)/core/timeouts.c \
    $(LWIPDIR)/core/udp.c \
    $(LWIPDIR)/core/ipv4/acd.c \
    $(LWIPDIR)/core/ipv4/autoip.c \
    $(LWIPDIR)/core/ipv4/dhcp.c \
    $(LWIPDIR)/core/ipv4/etharp.c \
    $(LWIPDIR)/core/ipv4/icmp.c \
    $(LWIPDIR)/core/ipv4/igmp.c \
    $(LWIPDIR)/core/ipv4/ip4_frag.c \
    $(LWIPDIR)/core/ipv4/ip4.c \
    $(LWIPDIR)/core/ipv4/ip4_addr.c \
    $(LWIPDIR)/core/ipv6/dhcp6.c \
    $(LWIPDIR)/core/ipv6/ethip6.c \
    $(LWIPDIR)/core/ipv6/icmp6.c \
    $(LWIPDIR)/core/ipv6/inet6.c \
    $(LWIPDIR)/core/ipv6/ip6.c \
    $(LWIPDIR)/core/ipv6/ip6_addr.c \
    $(LWIPDIR)/core/ipv6/ip6_frag.c \
    $(LWIPDIR)/core/ipv6/mld6.c \
    $(LWIPDIR)/core/ipv6/nd6.c \
    $(LWIPDIR)/netif/ethernet.c

LWIP_PORT := \
    port/lwip/sys_arch.c \
    port/lwip/lwip_libc.c \
    port/lwip/netif/flint_netif.c \
    bsp/rpi4/genet.c

LWIP_SRCS := $(LWIP_CORE) $(LWIP_PORT)

# OS-mode (netconn/sockets) sources - added by the Makefile when LWIP_OS=1.
LWIP_API := \
    $(LWIPDIR)/api/api_lib.c \
    $(LWIPDIR)/api/api_msg.c \
    $(LWIPDIR)/api/err.c \
    $(LWIPDIR)/api/netbuf.c \
    $(LWIPDIR)/api/netdb.c \
    $(LWIPDIR)/api/netifapi.c \
    $(LWIPDIR)/api/sockets.c \
    $(LWIPDIR)/api/if_api.c \
    $(LWIPDIR)/api/tcpip.c

# coreMQTT (manifest 3) + FlintRTOS transport over lwIP sockets.
MQTT_DIR := third_party/coreMQTT/source
MQTT_INC := -I$(MQTT_DIR)/include -I$(MQTT_DIR)/interface -Iport/mqtt
MQTT_SRCS := \
    $(MQTT_DIR)/core_mqtt.c \
    $(MQTT_DIR)/core_mqtt_serializer.c \
    $(MQTT_DIR)/core_mqtt_state.c \
    port/mqtt/transport_lwip.c

# PTP (manifest 3) - IEEE 1588 ordinary-clock slave. Reuses ptpd's data model
# (third_party/ptpd) with a FlintRTOS dep layer. OS mode only (sockets).
PTP_INC := -Ithird_party/ptpd/src -Iport/ptp
PTP_SRCS := \
    port/ptp/ptp_time.c \
    port/ptp/ptp_msg.c \
    port/ptp/ptp_servo.c \
    port/ptp/ptp_clock.c \
    port/ptp/ptp_slave.c
