/*
 * FlintRTOS - Message Buffers (manifest 2, extension). Public API.
 * Discrete variable-length messages layered over a stream buffer (each message
 * is length-prefixed). ISR-to-task friendly. configUSE_MESSAGE_BUFFERS == 1.
 */
#ifndef FLINT_MESSAGE_BUFFER_H
#define FLINT_MESSAGE_BUFFER_H

#include "FlintRTOS.h"
#include "stream_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *MessageBufferHandle_t;

MessageBufferHandle_t xMessageBufferCreate(size_t xBufferSizeBytes);
void   vMessageBufferDelete(MessageBufferHandle_t xMessageBuffer);
size_t xMessageBufferSend(MessageBufferHandle_t xMessageBuffer, const void *pvTxData,
                          size_t xDataLengthBytes, TickType_t xTicksToWait);
size_t xMessageBufferReceive(MessageBufferHandle_t xMessageBuffer, void *pvRxData,
                             size_t xBufferLengthBytes, TickType_t xTicksToWait);

#ifdef __cplusplus
}
#endif

#endif /* FLINT_MESSAGE_BUFFER_H */
