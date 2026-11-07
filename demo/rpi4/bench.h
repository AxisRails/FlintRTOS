/*
 * FlintRTOS - context-switch benchmark (Raspberry Pi 4).
 *
 *   sem_wake : task L gives a semaphore that higher-priority task H is
 *              blocked on; measured from just before xSemaphoreGive() in L to
 *              H's first instruction after xSemaphoreTake() returns. Covers
 *              the kernel give path, the yield trap, the context save/restore
 *              and the scheduler decision - the latency of an event-driven
 *              task wake-up.
 *   yield    : taskYIELD() with no other task ready at that priority: the
 *              trap + full context save/restore + scheduler round trip.
 *
 * Runs once a few seconds after boot and again on request (MQTT "bench").
 */
#ifndef FLINT_BENCH_H
#define FLINT_BENCH_H

#include <stdint.h>
#include <stdbool.h>
#include "flint_timing.h"

typedef struct
{
    bool           valid;
    uint32_t       runs;
    FlintTimingAcc sem_wake;
    FlintTimingAcc yield;
} BenchResult;

void               bench_start(void);       /* create the tasks (from main) */
void               bench_request(void);     /* run again (any task context) */
const BenchResult *bench_result(void);

#endif /* FLINT_BENCH_H */
