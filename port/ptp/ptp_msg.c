/*
 * FlintRTOS - PTP message pack/unpack. Byte layout per IEEE 1588v2 / ptpd def/.
 */
#include "ptp_msg.h"

/* ---- big-endian byte helpers -------------------------------------------- */
static uint16_t be16(const uint8_t *p) { return (uint16_t)(((uint16_t)p[0] << 8) | p[1]); }
static uint32_t be32(const uint8_t *p)
{ return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static uint64_t be48(const uint8_t *p)
{
    uint64_t v = 0U;
    for (int i = 0; i < 6; i++) { v = (v << 8) | p[i]; }
    return v;
}
static uint64_t be64(const uint8_t *p)
{
    uint64_t v = 0U;
    for (int i = 0; i < 8; i++) { v = (v << 8) | p[i]; }
    return v;
}
static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void put48(uint8_t *p, uint64_t v)
{ for (int i = 5; i >= 0; i--) { p[i] = (uint8_t)v; v >>= 8; } }
static void put32(uint8_t *p, uint32_t v)
{ p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }

void ptp_unpack_timestamp(const uint8_t *buf, Timestamp *ts)
{
    uint64_t secs = be48(&buf[0]);
    ts->secondsField.lsb = (uint32_t)secs;
    ts->secondsField.msb = (uint16_t)(secs >> 32);
    ts->nanosecondsField = (UInteger32)be32(&buf[6]);
}

uint8_t ptp_unpack_header(const uint8_t *buf, MsgHeader *hdr)
{
    hdr->transportSpecific = (Nibble)(buf[0] >> 4);
    hdr->messageType       = (Enumeration4)(buf[0] & 0x0FU);
    hdr->versionPTP        = (UInteger4)(buf[1] & 0x0FU);
    hdr->messageLength     = be16(&buf[2]);
    hdr->domainNumber      = buf[4];
    hdr->flagField0        = buf[6];
    hdr->flagField1        = buf[7];
    {
        uint64_t cf = be64(&buf[8]);
        hdr->correctionField.lsb = (uint32_t)cf;
        hdr->correctionField.msb = (int32_t)(cf >> 32);
    }
    /* sourcePortIdentity: clockIdentity[8] @20, portNumber @28 */
    for (int i = 0; i < CLOCK_IDENTITY_LENGTH; i++)
    {
        hdr->sourcePortIdentity.clockIdentity[i] = buf[20 + i];
    }
    hdr->sourcePortIdentity.portNumber = be16(&buf[28]);
    hdr->sequenceId        = be16(&buf[30]);
    hdr->controlField      = buf[32];
    hdr->logMessageInterval = (Integer8)buf[33];
    return (uint8_t)hdr->messageType;
}

int ptp_header_two_step(const MsgHeader *hdr)
{
    return ((hdr->flagField0 & 0x02U) != 0U) ? 1 : 0;   /* twoStepFlag */
}

size_t ptp_pack_delay_req(uint8_t *buf, const MsgHeader *self,
                          uint16_t seqId, const Timestamp *originTs)
{
    for (size_t i = 0U; i < DELAY_REQ_LENGTH; i++) { buf[i] = 0U; }

    buf[0] = (uint8_t)PTP_DELAY_REQ;          /* transportSpecific=0 | type   */
    buf[1] = 0x02U;                            /* reserved0=0 | versionPTP=2   */
    put16(&buf[2], (uint16_t)DELAY_REQ_LENGTH);
    buf[4] = self->domainNumber;
    buf[6] = 0U; buf[7] = 0U;                  /* flags */
    /* correctionField @8..15 = 0 */
    for (int i = 0; i < CLOCK_IDENTITY_LENGTH; i++)
    {
        buf[20 + i] = self->sourcePortIdentity.clockIdentity[i];
    }
    put16(&buf[28], self->sourcePortIdentity.portNumber);
    put16(&buf[30], seqId);
    buf[32] = 0x01U;                           /* controlField: Delay_Req      */
    buf[33] = 0x7FU;                           /* logMessageInterval: not set  */

    /* originTimestamp @34 (10 bytes) */
    {
        uint64_t secs = ((uint64_t)originTs->secondsField.msb << 32) | originTs->secondsField.lsb;
        put48(&buf[34], secs);
    }
    put32(&buf[40], (uint32_t)originTs->nanosecondsField);

    return (size_t)DELAY_REQ_LENGTH;
}
