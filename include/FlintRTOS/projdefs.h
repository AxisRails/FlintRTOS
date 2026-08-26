/*
 * FlintRTOS - project-wide return codes and boolean constants.
 * Arch-neutral. (FreeRTOS-style API for familiarity; MISRA C:2023 TCB.)
 */
#ifndef FLINT_PROJDEFS_H
#define FLINT_PROJDEFS_H

#define pdFALSE  ((BaseType_t)0)
#define pdTRUE   ((BaseType_t)1)

#define pdPASS   (pdTRUE)
#define pdFAIL   (pdFALSE)

/* Task/queue timeout results. */
#define errQUEUE_EMPTY  ((BaseType_t)0)
#define errQUEUE_FULL   ((BaseType_t)0)

/* Dynamic-allocation failure (heap). */
#define errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY  (-1)

#endif /* FLINT_PROJDEFS_H */
