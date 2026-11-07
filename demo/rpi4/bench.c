/*
 * FlintRTOS - context-switch benchmark. See bench.h.
 */
#include "FlintRTOS.h"
#include "task.h"
#include "semphr.h"
#include "uart.h"
#include "bench.h"

#define BENCH_ITER      (1000U)
#define BENCH_PRIO_H    (configMAX_PRIORITIES - 3U)
#define BENCH_PRIO_L    (configMAX_PRIORITIES - 4U)

static SemaphoreHandle_t s_start = NULL;   /* request a run          */
static SemaphoreHandle_t s_wake  = NULL;   /* L -> H wake-up signal  */
static volatile uint64_t s_t0    = 0U;     /* L's timestamp before give */
static BenchResult       s_res;

static inline uint64_t cnt(void)
{
    uint64_t v;
    __asm__ volatile("isb\n\tmrs %0, cntpct_el0" : "=r"(v) :: "memory");
    return v;
}
static inline uint64_t freq(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(v));
    return v;
}

static void prvBenchHigh(void *pv)
{
    (void)pv;
    for (;;)
    {
        (void)xSemaphoreTake(s_wake, portMAX_DELAY);
        {
            uint64_t t1 = cnt();
            flint_timing_acc_add(&s_res.sem_wake, flint_timing_ticks_to_ns(t1 - s_t0, freq()));
        }
    }
}

static void prvBenchLow(void *pv)
{
    (void)pv;
    vTaskDelay(5000U);                       /* first run once the system settles */
    for (;;)
    {
        flint_timing_acc_reset(&s_res.sem_wake);
        flint_timing_acc_reset(&s_res.yield);

        for (uint32_t i = 0U; i < BENCH_ITER; i++)
        {
            s_t0 = cnt();
            (void)xSemaphoreGive(s_wake);    /* H preempts us right here */
        }
        for (uint32_t i = 0U; i < BENCH_ITER; i++)
        {
            uint64_t t0 = cnt();
            taskYIELD();
            flint_timing_acc_add(&s_res.yield, flint_timing_ticks_to_ns(cnt() - t0, freq()));
        }
        s_res.valid = true;
        s_res.runs++;
        uart_printf("[bench] %u x sem wake-up: min %u ns, mean %u ns, max %u ns\n",
                    (unsigned int)s_res.sem_wake.count, (unsigned int)s_res.sem_wake.min_ns,
                    (unsigned int)flint_timing_acc_mean(&s_res.sem_wake), (unsigned int)s_res.sem_wake.max_ns);
        uart_printf("[bench] %u x yield round trip: min %u ns, mean %u ns, max %u ns\n",
                    (unsigned int)s_res.yield.count, (unsigned int)s_res.yield.min_ns,
                    (unsigned int)flint_timing_acc_mean(&s_res.yield), (unsigned int)s_res.yield.max_ns);

        (void)xSemaphoreTake(s_start, portMAX_DELAY);   /* wait for the next request */
    }
}

void bench_start(void)
{
    s_start = xSemaphoreCreateBinary();
    s_wake  = xSemaphoreCreateBinary();
    if ((s_start == NULL) || (s_wake == NULL)) { return; }
    (void)xTaskCreate(prvBenchHigh, "bench_h", (uint32_t)configMINIMAL_STACK_SIZE, NULL, BENCH_PRIO_H, NULL);
    (void)xTaskCreate(prvBenchLow,  "bench_l", (uint32_t)configMINIMAL_STACK_SIZE, NULL, BENCH_PRIO_L, NULL);
}

void bench_request(void)
{
    if (s_start != NULL) { (void)xSemaphoreGive(s_start); }
}

const BenchResult *bench_result(void)
{
    return &s_res;
}
