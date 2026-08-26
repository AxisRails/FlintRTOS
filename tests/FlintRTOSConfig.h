/*
 * FlintRTOSConfig.h - minimal configuration for host unit tests of the
 * portable core modules (list.c, queue.c, heap_*.c).
 */
#ifndef FLINTRTOS_CONFIG_H
#define FLINTRTOS_CONFIG_H

#define configTICK_RATE_HZ               (1000U)
#define configMAX_PRIORITIES             (32U)
#define configMINIMAL_STACK_SIZE         (256U)
#define configUSE_16_BIT_TICKS           0
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configTOTAL_HEAP_SIZE            (64U * 1024U)
#define configHEAP_ALGORITHM             4
#define configUSE_MALLOC_FAILED_HOOK     0

#endif /* FLINTRTOS_CONFIG_H */
