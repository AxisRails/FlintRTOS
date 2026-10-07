/*
 * FlintRTOS - MQTT device application (coreMQTT). See flint_mqtt.h.
 */
#include "flint_mqtt.h"
#include "mqtt_transport.h"
#include "mqtt_platform.h"
#include "jsonw.h"

#include "core_mqtt.h"

#define TOPIC_MAX          (64U)
#define ID_MAX             (32U)
#define HOST_MAX           (64U)
#define NET_BUF_SIZE       (2048U)
#define CONNECT_TIMEOUT_MS (8000U)
#define KEEPALIVE_S        (30U)
#define PTP_PERIOD_MS      (1000U)
#define STATS_PERIOD_MS    (10000U)
#define BACKOFF_MIN_MS     (2000U)
#define BACKOFF_MAX_MS     (60000U)
#define REPLY_MAX          (1024U)

typedef struct
{
    char      id[ID_MAX];
    char      host[HOST_MAX];
    uint16_t  port;
    bool      inited;

    MQTTContext_t        mqtt;
    TransportInterface_t transport;
    uint8_t              netbuf[NET_BUF_SIZE];
    MQTTFixedBuffer_t    fixed;

    bool      connected;
    uint32_t  next_attempt_ms;
    uint32_t  backoff_ms;
    uint32_t  last_ptp_ms;
    uint32_t  last_stats_ms;
    uint32_t  boot_ms;

    char      topic_buf[TOPIC_MAX];
    char      t_status[TOPIC_MAX];
    char      t_ptp[TOPIC_MAX];
    char      t_stats[TOPIC_MAX];
    char      t_cmd[TOPIC_MAX];
    char      t_reply[TOPIC_MAX];

    bool      reply_pending;
    char      reply[REPLY_MAX];
    char      scratch[REPLY_MAX];

    FlintMqttStats st;
} FlintMqtt;

static FlintMqtt g;

static void logf2(const char *a, const char *b)
{
    char line[160];
    fm_strlcpy(line, "[mqtt] ", sizeof(line));
    fm_strlcat(line, a, sizeof(line));
    if (b != NULL) { fm_strlcat(line, b, sizeof(line)); }
    fm_plat_log(line);
}

static void make_topic(char *dst, const char *leaf)
{
    fm_strlcpy(dst, "flint/", TOPIC_MAX);
    fm_strlcat(dst, g.id, TOPIC_MAX);
    fm_strlcat(dst, "/", TOPIC_MAX);
    fm_strlcat(dst, leaf, TOPIC_MAX);
}

const char *flint_mqtt_topic(const char *leaf)
{
    make_topic(g.topic_buf, leaf);
    return g.topic_buf;
}

const FlintMqttStats *flint_mqtt_stats(void) { return &g.st; }
bool flint_mqtt_connected(void)              { return g.connected; }

static uint32_t now_ms(void) { return fm_plat_ms(); }

/* ---- Publishing ------------------------------------------------------------ */
static bool publish(const char *topic, const char *payload, size_t len, bool retain)
{
    MQTTPublishInfo_t pi;
    MQTTStatus_t      rc;

    if (!g.connected) { return false; }
    pi.qos             = MQTTQoS0;
    pi.retain          = retain;
    pi.dup             = false;
    pi.pTopicName      = topic;
    pi.topicNameLength = (uint16_t)fm_strlen(topic);
    pi.pPayload        = payload;
    pi.payloadLength   = len;
    rc = MQTT_Publish(&g.mqtt, &pi, 0U);
    if (rc == MQTTSuccess) { g.st.published++; return true; }
    logf2("publish failed: ", MQTT_Status_strerror(rc));
    return false;
}

static void build_status(JsonW *w, const char *state)
{
    jw_init(w, g.scratch, sizeof(g.scratch));
    jw_obj_open(w);
    jw_str(w, "state", state);
    jw_str(w, "id", g.id);
    jw_str(w, "ip", fm_plat_ip());
    jw_str(w, "fw", "FlintRTOS / lwIP 2.2.0 / coreMQTT 2.3.1");
    jw_u32(w, "uptime_s", (now_ms() - g.boot_ms) / 1000U);
    jw_obj_close(w);
}

static void publish_status_online(void)
{
    JsonW w;
    build_status(&w, "online");
    (void)publish(g.t_status, g.scratch, jw_len(&w), true);
}

static void publish_ptp(void)
{
    size_t n = fm_plat_ptp_json(g.scratch, sizeof(g.scratch));
    if (n > 0U) { (void)publish(g.t_ptp, g.scratch, n, false); }
}

static void publish_stats(void)
{
    size_t n = fm_plat_stats_json(g.scratch, sizeof(g.scratch));
    if (n > 0U) { (void)publish(g.t_stats, g.scratch, n, false); }
}

/* ---- Commands -------------------------------------------------------------- */
static bool word_eq(const char *a, size_t alen, const char *b)
{
    size_t n = fm_strlen(b);
    if (alen != n) { return false; }
    for (size_t i = 0U; i < n; i++) { if (a[i] != b[i]) { return false; } }
    return true;
}

static void set_reply_simple(const char *cmd, const char *k, const char *v)
{
    JsonW w;
    jw_init(&w, g.reply, sizeof(g.reply));
    jw_obj_open(&w);
    jw_str(&w, "cmd", cmd);
    jw_str(&w, k, v);
    jw_obj_close(&w);
}

/* Parse "verb [arg]" (trimmed, case-sensitive, lowercase). Runs from the MQTT
   event callback, so it only prepares the reply; it's published afterwards. */
static void handle_command(const char *p, size_t len)
{
    char cmd[48];
    size_t s = 0U, e = len, sp;

    while ((s < e) && ((p[s] == ' ') || (p[s] == '\n') || (p[s] == '\r') || (p[s] == '\t'))) { s++; }
    while ((e > s) && ((p[e - 1U] == ' ') || (p[e - 1U] == '\n') || (p[e - 1U] == '\r') || (p[e - 1U] == '\t'))) { e--; }
    {
        size_t n = ((e - s) < (sizeof(cmd) - 1U)) ? (e - s) : (sizeof(cmd) - 1U);
        for (size_t i = 0U; i < n; i++) { cmd[i] = p[s + i]; }
        cmd[n] = '\0';
        len = n;
    }
    for (sp = 0U; (sp < len) && (cmd[sp] != ' '); sp++) { }

    g.st.commands++;
    logf2("cmd: ", cmd);

    if (word_eq(cmd, len, "ping"))
    {
        set_reply_simple(cmd, "reply", "pong");
    }
    else if (word_eq(cmd, len, "uptime"))
    {
        JsonW w;
        jw_init(&w, g.reply, sizeof(g.reply));
        jw_obj_open(&w);
        jw_str(&w, "cmd", cmd);
        jw_u32(&w, "uptime_s", (now_ms() - g.boot_ms) / 1000U);
        jw_obj_close(&w);
    }
    else if (word_eq(cmd, len, "stats"))
    {
        if (fm_plat_stats_json(g.reply, sizeof(g.reply)) == 0U) { set_reply_simple(cmd, "error", "unavailable"); }
    }
    else if (word_eq(cmd, len, "ptp"))
    {
        if (fm_plat_ptp_json(g.reply, sizeof(g.reply)) == 0U) { set_reply_simple(cmd, "error", "PTP not running"); }
    }
    else if (word_eq(cmd, sp, "led") && (sp < len))
    {
        const char *arg = &cmd[sp + 1U];
        size_t      al  = len - sp - 1U;
        int         r;
        if      (word_eq(arg, al, "on"))     { r = fm_plat_led(1); }
        else if (word_eq(arg, al, "off"))    { r = fm_plat_led(0); }
        else if (word_eq(arg, al, "toggle")) { r = fm_plat_led(-1); }
        else                                 { r = -2; }
        if (r == -2) { set_reply_simple(cmd, "error", "use: led on|off|toggle"); }
        else         { set_reply_simple(cmd, "led", (r != 0) ? "on" : "off"); }
    }
    else if (word_eq(cmd, len, "help"))
    {
        set_reply_simple(cmd, "commands", "ping, uptime, stats, ptp, led on|off|toggle, help");
    }
    else
    {
        set_reply_simple(cmd, "error", "unknown command (try: help)");
    }
    g.reply_pending = true;
}

static bool topic_is(const MQTTPublishInfo_t *pi, const char *topic)
{
    return word_eq(pi->pTopicName, pi->topicNameLength, topic);
}

static void on_event(MQTTContext_t *ctx, MQTTPacketInfo_t *pkt, MQTTDeserializedInfo_t *info)
{
    (void)ctx;
    if (((pkt->type & 0xF0U) == MQTT_PACKET_TYPE_PUBLISH) && (info->pPublishInfo != NULL))
    {
        const MQTTPublishInfo_t *pi = info->pPublishInfo;
        if (topic_is(pi, g.t_cmd))
        {
            handle_command((const char *)pi->pPayload, pi->payloadLength);
        }
    }
    else if (pkt->type == MQTT_PACKET_TYPE_SUBACK)
    {
        logf2("subscribed to ", g.t_cmd);
    }
    else
    {
        /* PINGRESP etc.: nothing to do */
    }
}

/* ---- Connection management ------------------------------------------------- */
static void drop(const char *why)
{
    if (g.connected) { logf2("connection lost: ", why); }
    g.connected = false;
    mqtt_transport_close(mqtt_transport_ctx());
    g.next_attempt_ms = now_ms() + g.backoff_ms;
    g.backoff_ms = ((g.backoff_ms * 2U) > BACKOFF_MAX_MS) ? BACKOFF_MAX_MS : (g.backoff_ms * 2U);
}

static void try_connect(void)
{
    NetworkContext_t *net = mqtt_transport_ctx();
    MQTTConnectInfo_t ci;
    MQTTPublishInfo_t will;
    MQTTStatus_t      rc;
    bool              session = false;
    static const char k_offline[] = "{\"state\":\"offline\"}";

    logf2("connecting to ", g.host);
    if (mqtt_transport_open(net, g.host, g.port, CONNECT_TIMEOUT_MS) != 0)
    {
        g.st.connect_failures++;
        logf2("TCP/DNS failed: ", mqtt_transport_error(net));
        drop("open");
        return;
    }

    g.transport.pNetworkContext = net;
    g.transport.send   = mqtt_transport_send;
    g.transport.recv   = mqtt_transport_recv;
    g.transport.writev = NULL;
    g.fixed.pBuffer = g.netbuf;
    g.fixed.size    = sizeof(g.netbuf);
    rc = MQTT_Init(&g.mqtt, &g.transport, now_ms, on_event, &g.fixed);
    if (rc != MQTTSuccess) { logf2("MQTT_Init: ", MQTT_Status_strerror(rc)); drop("init"); return; }

    ci.cleanSession           = true;
    ci.pClientIdentifier      = g.id;
    ci.clientIdentifierLength = (uint16_t)fm_strlen(g.id);
    ci.keepAliveSeconds       = KEEPALIVE_S;
    ci.pUserName = NULL; ci.userNameLength = 0U;
    ci.pPassword = NULL; ci.passwordLength = 0U;

    will.qos             = MQTTQoS0;
    will.retain          = true;
    will.dup             = false;
    will.pTopicName      = g.t_status;
    will.topicNameLength = (uint16_t)fm_strlen(g.t_status);
    will.pPayload        = k_offline;
    will.payloadLength   = sizeof(k_offline) - 1U;

    rc = MQTT_Connect(&g.mqtt, &ci, &will, CONNECT_TIMEOUT_MS, &session);
    if (rc != MQTTSuccess)
    {
        g.st.connect_failures++;
        logf2("CONNECT rejected/failed: ", MQTT_Status_strerror(rc));
        drop("connect");
        return;
    }
    g.connected  = true;
    g.backoff_ms = BACKOFF_MIN_MS;
    g.st.connects++;
    logf2("connected as ", g.id);

    {
        MQTTSubscribeInfo_t sub;
        sub.qos               = MQTTQoS0;
        sub.pTopicFilter      = g.t_cmd;
        sub.topicFilterLength = (uint16_t)fm_strlen(g.t_cmd);
        rc = MQTT_Subscribe(&g.mqtt, &sub, 1U, MQTT_GetPacketId(&g.mqtt));
        if (rc != MQTTSuccess) { logf2("SUBSCRIBE failed: ", MQTT_Status_strerror(rc)); }
    }
    publish_status_online();
    logf2("topics under flint/", g.id);
    g.last_ptp_ms   = now_ms();
    g.last_stats_ms = now_ms() - STATS_PERIOD_MS + 2000U;   /* first stats in 2 s */
}

/* ---- Public ---------------------------------------------------------------- */
void flint_mqtt_init(const char *client_id, const char *broker_host, uint16_t broker_port)
{
    fm_strlcpy(g.id, client_id, sizeof(g.id));
    fm_strlcpy(g.host, broker_host, sizeof(g.host));
    g.port = broker_port;
    make_topic(g.t_status, "status");
    make_topic(g.t_ptp,    "ptp");
    make_topic(g.t_stats,  "stats");
    make_topic(g.t_cmd,    "cmd");
    make_topic(g.t_reply,  "reply");
    g.boot_ms         = now_ms();
    g.backoff_ms      = BACKOFF_MIN_MS;
    g.next_attempt_ms = now_ms();
    g.connected       = false;
    g.inited          = true;
    logf2("client id ", g.id);
}

void flint_mqtt_service(void)
{
    uint32_t t;

    if (!g.inited) { return; }
    t = now_ms();

    if (!g.connected)
    {
        if ((int32_t)(t - g.next_attempt_ms) >= 0) { try_connect(); }
        return;
    }

    {
        MQTTStatus_t rc = MQTT_ProcessLoop(&g.mqtt);
        if ((rc != MQTTSuccess) && (rc != MQTTNeedMoreBytes))
        {
            drop(MQTT_Status_strerror(rc));
            return;
        }
        if (!mqtt_transport_is_up(mqtt_transport_ctx()))
        {
            drop(mqtt_transport_error(mqtt_transport_ctx()));
            return;
        }
    }

    if (g.reply_pending)
    {
        g.reply_pending = false;
        (void)publish(g.t_reply, g.reply, fm_strlen(g.reply), false);
    }

    t = now_ms();
    if ((uint32_t)(t - g.last_ptp_ms) >= PTP_PERIOD_MS)
    {
        g.last_ptp_ms += PTP_PERIOD_MS;
        if ((uint32_t)(t - g.last_ptp_ms) >= PTP_PERIOD_MS) { g.last_ptp_ms = t; }  /* don't burst */
        publish_ptp();
    }
    if ((uint32_t)(t - g.last_stats_ms) >= STATS_PERIOD_MS)
    {
        g.last_stats_ms = t;
        publish_stats();
    }
}
