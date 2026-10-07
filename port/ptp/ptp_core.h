/*
 * FlintRTOS - IEEE 1588-2008 (PTPv2) ordinary-clock SLAVE, transport-neutral.
 *
 * The engine is fed received PTP messages together with their receive
 * timestamps (raw counter ticks, ideally taken in the driver) and sends
 * Delay_Req through a callback that reports the transmit timestamp. This keeps
 * one protocol implementation for both lwIP NO_SYS (raw UDP, driver
 * timestamps) and lwIP OS mode (sockets).
 *
 * Profile: default E2E delay mechanism over UDP/IPv4 multicast (224.0.1.129,
 * event port 319, general port 320), one- or two-step masters, domain 0. It is
 * what linuxptp's ptp4l uses out of the box. Master selection is the Announce
 * dataset comparison of the IEEE 1588 BMC algorithm, with receipt timeout.
 */
#ifndef FLINT_PTP_CORE_H
#define FLINT_PTP_CORE_H

#include <stdint.h>
#include <stdbool.h>

/* Transmit hook: send len bytes to PTP multicast on UDP dst port `port`.
   On success store the frame's transmit timestamp (counter ticks) in *tx_cnt. */
typedef bool (*PtpTxFn)(uint16_t port, const uint8_t *buf, uint16_t len, uint64_t *tx_cnt);

void ptp_core_init(const uint8_t mac[6], uint8_t domain, PtpTxFn tx);

/* Deliver one received message. port = UDP destination port (319 or 320),
   rx_cnt = receive timestamp in counter ticks. */
void ptp_core_rx(const uint8_t *buf, uint16_t len, uint16_t port, uint64_t rx_cnt);

/* Periodic work (send Delay_Req, master timeout). Call every few ms. */
void ptp_core_poll(uint32_t now_ms);

/* Current state for applications. */
bool    ptp_core_is_locked(void);
int64_t ptp_core_last_offset_ns(void);

/* Snapshot for telemetry (e.g. MQTT). */
typedef struct
{
    bool        have_master;
    bool        locked;
    const char *state;          /* LISTENING / UNLOCKED / STEPPED / LOCKED */
    int64_t     offset_ns;      /* last offset from master                 */
    int64_t     delay_ns;       /* filtered mean path delay (-1 unknown)   */
    int32_t     freq_ppb;       /* applied frequency correction            */
    uint32_t    n_sync;         /* Sync messages processed                 */
    uint8_t     gm_id[8];       /* grandmaster clockIdentity               */
    int16_t     utc_offset;
} PtpStatus;
void ptp_core_get_status(PtpStatus *st);

/* Current time in UTC nanoseconds (valid once locked). */
int64_t ptp_core_utc_now_ns(void);

/* Format UTC ns as "YYYY-MM-DD hh:mm:ss.uuuuuu" (buf >= 27 bytes). */
void    ptp_format_utc(int64_t utc_ns, char *buf);

#endif /* FLINT_PTP_CORE_H */
