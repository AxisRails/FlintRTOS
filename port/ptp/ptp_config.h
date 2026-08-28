/*
 * FlintRTOS - PTP port: exposes ptpd's IEEE-1588 data model bare-metal.
 * Reuses ptpd's exact wire structures/constants (third_party/ptpd/src) via a
 * one-line shim (the daemon's dep/ layer is replaced by port/ptp/).
 */
#ifndef FLINT_PTP_CONFIG_H
#define FLINT_PTP_CONFIG_H

#include <stdint.h>
#include <stddef.h>

/* ptpd puts this in dep/constants_dep.h (which is OS-coupled); supply it here. */
#ifndef CLOCK_IDENTITY_LENGTH
#define CLOCK_IDENTITY_LENGTH 8
#endif

#include "ptp_primitives.h"     /* ptpd: Integer8..64, Octet, Boolean, ...     */
#include "constants.h"          /* ptpd: message lengths, intervals, enums     */
#include "ptp_datatypes.h"      /* ptpd: MsgHeader, MsgSync, Timestamp, ...     */

/* PTP-over-UDP transport constants (IEEE 1588 Annex D). */
#define PTP_EVENT_PORT      319U
#define PTP_GENERAL_PORT    320U
#define PTP_MCAST_ADDR      "224.0.1.129"   /* primary (non-peer) multicast    */

/* messageType values (IEEE 1588 Table 19). */
#define PTP_SYNC            0x0U
#define PTP_DELAY_REQ       0x1U
#define PTP_FOLLOW_UP       0x8U
#define PTP_DELAY_RESP      0x9U
#define PTP_ANNOUNCE        0xBU

#define PTP_HEADER_LENGTH   34U

#endif /* FLINT_PTP_CONFIG_H */
