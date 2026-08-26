/*
 * FlintRTOS - Stream Buffers (manifest 2). TCB code (MISRA C:2023).
 * Skeleton: a circular byte buffer with head/tail indices and a trigger level;
 * ISR-safe send path with a task-notify wake. Body lands with task notifications.
 * Compiles to nothing unless configUSE_STREAM_BUFFERS == 1.
 */
#include "FlintRTOS.h"

#if (configUSE_STREAM_BUFFERS == 1)

#include "stream_buffer.h"

/* TODO(next increment): circular buffer + trigger level + notify-on-data.
   message_buffer.h layers length-prefixed messages over this. */

#endif /* configUSE_STREAM_BUFFERS */
