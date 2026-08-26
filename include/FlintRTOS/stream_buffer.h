/*
 * FlintRTOS - Stream Buffers (manifest 2, extension). Public API.
 * Lock-light single-reader/single-writer byte stream, optimised for passing
 * data from an ISR to a task. configUSE_STREAM_BUFFERS == 1.
 */
#ifndef FLINT_STREAM_BUFFER_H
#define FLINT_STREAM_BUFFER_H

#include "FlintRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *StreamBufferHandle_t;

StreamBufferHandle_t xStreamBufferCreate(size_t xBufferSizeBytes,
                                         size_t xTriggerLevelBytes);
void   vStreamBufferDelete(StreamBufferHandle_t xStreamBuffer);
size_t xStreamBufferSend(StreamBufferHandle_t xStreamBuffer, const void *pvTxData,
                         size_t xDataLengthBytes, TickType_t xTicksToWait);
size_t xStreamBufferReceive(StreamBufferHandle_t xStreamBuffer, void *pvRxData,
                            size_t xBufferLengthBytes, TickType_t xTicksToWait);
size_t xStreamBufferBytesAvailable(StreamBufferHandle_t xStreamBuffer);
size_t xStreamBufferSpacesAvailable(StreamBufferHandle_t xStreamBuffer);

#ifdef __cplusplus
}
#endif

#endif /* FLINT_STREAM_BUFFER_H */
