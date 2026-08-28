/*
 * FlintRTOS - lwIP OS-mode + coreMQTT demo (RPi4). Compiled with `make LWIP_OS=1`.
 *
 * Brings the TCP/IP stack up under the tcpip thread, then a network task
 * connects to an MQTT broker over lwIP sockets (coreMQTT) and publishes a
 * status message. This exercises the full stack: sys_arch (sem/mbox/thread) ->
 * netconn/sockets -> coreMQTT. Build-verified; runtime needs the GENET driver
 * (packets) and a reachable broker.
 */
#include "FlintRTOS.h"

#ifdef FLINT_LWIP_OS

#include "task.h"
#include "uart.h"

#include "lwip/tcpip.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "lwip/dhcp.h"
#include "flint_netif.h"

#if (configUSE_PTP == 1)
#include "ptp_slave.h"
#endif
#include "core_mqtt.h"
#include "transport_lwip.h"

static struct netif s_netif;

/* ---- coreMQTT glue ------------------------------------------------------- */
static uint32_t mqtt_now_ms(void)
{
    return (uint32_t)xTaskGetTickCount();
}

static void mqtt_event_cb(MQTTContext_t *pCtx, MQTTPacketInfo_t *pPacketInfo,
                          MQTTDeserializedInfo_t *pDeserializedInfo)
{
    (void)pCtx;
    (void)pPacketInfo;
    (void)pDeserializedInfo;
    /* Incoming PUBLISH / ACK handling would go here. */
}

static void mqtt_demo(void)
{
    NetworkContext_t net = { -1 };
    MQTTContext_t    mqtt;
    TransportInterface_t transport;
    static uint8_t   mqttBuffer[1024];
    MQTTFixedBuffer_t fixedBuffer = { mqttBuffer, sizeof(mqttBuffer) };
    MQTTConnectInfo_t connectInfo;
    MQTTPublishInfo_t publishInfo;
    bool sessionPresent = false;

    if (transport_connect(&net, "192.168.1.10", 1883) != 0)
    {
        uart_printf("[mqtt] TCP connect to broker failed\n");
        return;
    }

    transport.pNetworkContext = &net;
    transport.send = transport_send;
    transport.recv = transport_recv;
    transport.writev = NULL;

    if (MQTT_Init(&mqtt, &transport, mqtt_now_ms, mqtt_event_cb, &fixedBuffer) != MQTTSuccess)
    {
        uart_printf("[mqtt] MQTT_Init failed\n");
        transport_disconnect(&net);
        return;
    }

    connectInfo.cleanSession = true;
    connectInfo.pClientIdentifier = "flintrtos";
    connectInfo.clientIdentifierLength = 9U;
    connectInfo.keepAliveSeconds = 60U;
    connectInfo.pUserName = NULL;  connectInfo.userNameLength = 0U;
    connectInfo.pPassword = NULL;  connectInfo.passwordLength = 0U;

    if (MQTT_Connect(&mqtt, &connectInfo, NULL, 5000U, &sessionPresent) != MQTTSuccess)
    {
        uart_printf("[mqtt] MQTT_Connect failed\n");
        transport_disconnect(&net);
        return;
    }
    uart_printf("[mqtt] connected; publishing flint/status = \"up\"\n");

    publishInfo.qos = MQTTQoS0;
    publishInfo.retain = false;
    publishInfo.dup = false;
    publishInfo.pTopicName = "flint/status";
    publishInfo.topicNameLength = 12U;
    publishInfo.pPayload = "up";
    publishInfo.payloadLength = 2U;

    (void)MQTT_Publish(&mqtt, &publishInfo, 0U);   /* QoS0 -> packet id 0 */

    for (;;)
    {
        (void)MQTT_ProcessLoop(&mqtt);
        vTaskDelay(100U);
    }
}

void vNetworkTaskOS(void *pvParameters)
{
    (void)pvParameters;

    /* Start the tcpip thread (sys_arch-backed). */
    tcpip_init(NULL, NULL);

    {
        ip4_addr_t ipaddr, netmask, gw;
        IP4_ADDR(&ipaddr,  0, 0, 0, 0);      /* 0.0.0.0 -> DHCP fills it in */
        IP4_ADDR(&netmask, 0, 0, 0, 0);
        IP4_ADDR(&gw,      0, 0, 0, 0);

        LOCK_TCPIP_CORE();
        (void)netif_add(&s_netif, &ipaddr, &netmask, &gw, NULL,
                        flint_netif_init, tcpip_input);
        netif_set_default(&s_netif);
        netif_set_up(&s_netif);
        (void)dhcp_start(&s_netif);
        UNLOCK_TCPIP_CORE();
    }

#if (configUSE_PTP == 1)
    (void)xTaskCreate(vPtpTask, "ptp", (uint32_t)(configMINIMAL_STACK_SIZE * 4U), NULL, 6U, NULL);
    uart_printf("[net-os] PTP slave task started\n");
#endif
    uart_printf("[net-os] tcpip up; DHCP started; starting MQTT client...\n");
    mqtt_demo();

    for (;;) { vTaskDelay(1000U); }
}

#endif /* FLINT_LWIP_OS */
