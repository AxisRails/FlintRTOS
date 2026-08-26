/*
 * FlintRTOSConfig.h - RPi4 demo configuration.
 *
 * The single place an application tunes the kernel and enables/disables
 * modules (manifest section 4). Extension modules compile in only when their
 * configUSE_* switch is 1 (pay-for-what-you-use, A-G1).
 */
#ifndef FLINTRTOS_CONFIG_H
#define FLINTRTOS_CONFIG_H

/* ---- Core scheduler -------------------------------------------------------*/
#define configUSE_PREEMPTION            1
#define configCPU_CLOCK_HZ              (1500000000U)  /* Cortex-A72 ~1.5 GHz */
#define configTICK_RATE_HZ             (1000U)         /* 1 ms tick           */
#define configMAX_PRIORITIES           (32U)
#define configMINIMAL_STACK_SIZE       (1024U)         /* StackType_t words   */
#define configMAX_TASK_NAME_LEN        (16)
#define configUSE_16_BIT_TICKS         0
#define configIDLE_SHOULD_YIELD        1
#define configUSE_TIME_SLICING         1

/* ---- Memory management (manifest 2: MemMang) ------------------------------*/
/* Selects which portable/MemMang/heap_N.c the build links (1..5). */
#define configHEAP_ALGORITHM           4
#define configTOTAL_HEAP_SIZE          (1024U * 1024U)  /* 1 MiB kernel heap  */
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configSUPPORT_STATIC_ALLOCATION  1

/* ---- Synchronisation primitives (built on queue.c) ------------------------*/
#define configUSE_MUTEXES              1
#define configUSE_RECURSIVE_MUTEXES    1
#define configUSE_COUNTING_SEMAPHORES  1
#define configQUEUE_REGISTRY_SIZE      8

/* ---- Kernel extension modules (manifest 2) --------------------------------*/
#define configUSE_TIMERS               1   /* timers.c        */
#define configTIMER_TASK_PRIORITY      (configMAX_PRIORITIES - 1U)
#define configTIMER_QUEUE_LENGTH       10
#define configTIMER_TASK_STACK_DEPTH   (configMINIMAL_STACK_SIZE * 2U)
#define configUSE_EVENT_GROUPS         1   /* event_groups.c  */
#define configUSE_STREAM_BUFFERS       1   /* stream_buffer.c */
#define configUSE_MESSAGE_BUFFERS      1   /* message_buffer.c*/
#define configUSE_CO_ROUTINES          0   /* croutine.c (legacy, off) */

/* ---- Hooks / diagnostics --------------------------------------------------*/
#define configUSE_IDLE_HOOK            0
#define configUSE_TICK_HOOK            0
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configUSE_MALLOC_FAILED_HOOK   1

/* ---- Ecosystem libraries (manifest 3) - integrated when present -----------*/
#define configUSE_LWIP                 0   /* FlintRTOS+LwIP TCP/IP           */
#define configUSE_CLI                  0   /* FlintRTOS-CLI over UART         */
#define configUSE_CORE_MQTT            0   /* coreMQTT client                */
#define configUSE_CORE_HTTP            0   /* coreHTTP client                */
#define configUSE_PTP                  0   /* IEEE 1588 PTP                  */

#endif /* FLINTRTOS_CONFIG_H */
