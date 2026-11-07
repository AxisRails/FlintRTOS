/*
 * FlintRTOS - safety services (TCB, MISRA C:2023).
 *
 *  1. Crash record that survives a reset. A small RAM area outside the image
 *     (configSAFETY_NOINIT_ADDR, never zeroed by start.S) holds:
 *       - a header (magic, version, boot count) protected by a CRC,
 *       - "last activity" (task switched in + tick), updated on every
 *         context switch so a hang with interrupts off can still be traced,
 *       - the crash itself (reason, ESR/ELR/FAR/SPSR, LR, frame-pointer
 *         backtrace, task, tick, detail text), CRC-protected.
 *     At boot flint_safety_boot() classifies the previous reset and keeps a
 *     copy for reporting (UART now, MQTT once the network is up).
 *
 *  2. flint_panic(): the single fatal-error path. Records, prints, then resets
 *     the board through the hardware watchdog (configSAFETY_RESET_ON_FAULT=1)
 *     or halts (0). CPU exceptions, stack overflow, out-of-memory, lwIP
 *     assertions and hung tasks all end here.
 *
 *  3. Task health monitor: tasks register with a deadline and check in; a
 *     top-priority monitor task feeds the hardware watchdog only while every
 *     registered task is on time. A late task is named in the crash record
 *     before the reset. If the monitor itself cannot run (interrupts left
 *     masked, a runaway top-priority task), the hardware watchdog fires on its
 *     own and the next boot reports "watchdog reset" with the last task seen.
 */
#ifndef FLINT_SAFETY_H
#define FLINT_SAFETY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    FLINT_RESET_COLD = 0,       /* no valid record: power-on or RAM not kept */
    FLINT_RESET_WATCHDOG,       /* was running, no crash logged: HW watchdog */
    FLINT_RESET_CPU_FAULT,      /* unhandled exception (abort, undefined...) */
    FLINT_RESET_STACK_OVERFLOW,
    FLINT_RESET_TASK_HUNG,      /* a monitored task missed its deadline */
    FLINT_RESET_OUT_OF_MEMORY,
    FLINT_RESET_ASSERT,         /* failed assertion (e.g. lwIP) */
    FLINT_RESET_REQUESTED,      /* software reboot (command) */
    FLINT_RESET_REASON_COUNT
} FlintResetReason;

#define FLINT_SAFETY_BT_DEPTH    (8U)
#define FLINT_SAFETY_NAME_LEN    (16U)
#define FLINT_SAFETY_DETAIL_LEN  (48U)

/* CPU state captured for an exception (zeros when not applicable). */
typedef struct
{
    uint64_t esr, elr, far, spsr, lr, fp, sp;
} FlintCpuState;

/* What the previous run left behind (decoded, for reports). */
typedef struct
{
    FlintResetReason reason;
    uint32_t boot_count;        /* this boot's number (1 = first retained) */
    uint32_t pm_rsts;           /* raw PM_RSTS reset-status register */
    bool     have_activity;     /* last_task/last_tick valid */
    char     last_task[FLINT_SAFETY_NAME_LEN];
    uint32_t last_tick;
    /* crash details (reason >= FLINT_RESET_CPU_FAULT) */
    char     task[FLINT_SAFETY_NAME_LEN];
    uint32_t tick;
    FlintCpuState cpu;
    uint64_t backtrace[FLINT_SAFETY_BT_DEPTH];
    uint32_t bt_depth;
    char     detail[FLINT_SAFETY_DETAIL_LEN];
} FlintResetReport;

/* ---- Boot / reporting ----------------------------------------------------- */
/* Validate and decode the record at noinit_area, then re-arm it for this run.
   Call once, early (after the MMU is on, before tasks run). */
void flint_safety_boot(void *noinit_area, uint32_t pm_rsts);
const FlintResetReport *flint_safety_last_reset(void);
const char *flint_safety_reason_name(FlintResetReason r);
void flint_safety_print_report(void);

/* ---- Fatal path ----------------------------------------------------------- */
void flint_panic(FlintResetReason reason, const char *detail,
                 const FlintCpuState *cpu) __attribute__((noreturn));

/* Same, naming the task explicitly (the monitor blames the late task). */
void flint_panic_task(FlintResetReason reason, const char *task, const char *detail,
                      const FlintCpuState *cpu) __attribute__((noreturn));

/* Scheduler hook (traceTASK_SWITCHED_IN): record the incoming task. */
void flint_safety_note_switch(const char *task_name, uint32_t tick);

/* ---- Task health monitor -------------------------------------------------- */
#define FLINT_HEALTH_MAX  (8U)
/* Register the calling context's task: it must check in at least every
   deadline_ms. Returns a handle >= 0, or -1 if the table is full. */
int  flint_health_register(const char *name, uint32_t deadline_ms);
void flint_health_checkin(int handle);
/* Start the monitor task (highest priority) and tighten the HW watchdog. */
void flint_health_start(void);
/* Monitor body, exposed for host tests: checks all deadlines at now_ms;
   returns the index of the first late task, or -1 (and feeds the watchdog). */
int  flint_health_check(uint32_t now_ms);
const char *flint_health_name(int handle);
uint32_t    flint_health_silence_ms(int handle, uint32_t now_ms);
uint32_t    flint_health_deadline_ms(int handle);

/* Bounded string building without printf (returns the new length). */
size_t flint_safety_append(char *dst, size_t cap, size_t pos, const char *s);
size_t flint_safety_append_u32(char *dst, size_t cap, size_t pos, uint32_t v);

/* ---- Platform hooks (implemented per port; stubbed in host tests) --------- */
uint32_t flint_safety_port_ms(void);
const char *flint_safety_port_task_name(void);
void flint_safety_port_wdog_start(uint32_t ms);
void flint_safety_port_wdog_feed(void);
void flint_safety_port_reset(void) __attribute__((noreturn));
void flint_safety_port_print(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* FLINT_SAFETY_H */
