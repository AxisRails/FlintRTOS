/*
 * FlintRTOS - PTP message pack/unpack (IEEE 1588v2 over UDP).
 * Parses/builds the on-wire bytes into ptpd's structs (MsgHeader, Timestamp,
 * PortIdentity). Field offsets/sizes follow ptpd's def/message tree.
 */
#ifndef FLINT_PTP_MSG_H
#define FLINT_PTP_MSG_H

#include "ptp_config.h"
#include <stdint.h>
#include <stddef.h>

/* Unpack the 34-byte common header. Returns messageType (low nibble). */
uint8_t ptp_unpack_header(const uint8_t *buf, MsgHeader *hdr);

/* Unpack a 10-byte Timestamp at buf. */
void    ptp_unpack_timestamp(const uint8_t *buf, Timestamp *ts);

/* True if the two-step flag is set in the header's flagField. */
int     ptp_header_two_step(const MsgHeader *hdr);

/*
 * Build a Delay_Req (44 bytes) into buf. domain/seqId/sourcePortIdentity are
 * taken from *self; originTimestamp carries our TX estimate. Returns length.
 */
size_t  ptp_pack_delay_req(uint8_t *buf, const MsgHeader *self,
                           uint16_t seqId, const Timestamp *originTs);

#endif /* FLINT_PTP_MSG_H */
