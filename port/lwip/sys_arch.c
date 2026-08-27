/*
 * FlintRTOS - lwIP port: sys_arch.
 *   - Common (both modes): sys_now, lightweight protection, PRNG.
 *   - OS mode (NO_SYS==0): semaphores, mutexes, mailboxes, and threads mapped
 *     onto FlintRTOS semaphores/queues/tasks. This is what enables the sequential
 *     (netconn) and BSD sockets APIs, and hence coreMQTT/coreHTTP.
 */
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/err.h"

#include <stdint.h>

/* ---- Common ------------------------------------------------------------- */
extern uint32_t xTaskGetTickCount(void);

u32_t sys_now(void)
{
    return (u32_t)xTaskGetTickCount();
}

sys_prot_t sys_arch_protect(void)
{
    uint64_t daif;
    __asm__ volatile("mrs %0, daif" : "=r"(daif));
    __asm__ volatile("msr daifset, #2" ::: "memory");
    return (sys_prot_t)daif;
}

void sys_arch_unprotect(sys_prot_t pval)
{
    __asm__ volatile("msr daif, %0" :: "r"((uint64_t)pval) : "memory");
}

static uint32_t s_rng = 0x2545F491U;
unsigned int lwip_port_rand(void)
{
    uint32_t x = s_rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    s_rng = x;
    return (unsigned int)x;
}

/* ======================= OS MODE (NO_SYS == 0) ========================== */
#if !NO_SYS

#include "FlintRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

void sys_init(void) { }

/* ---- Semaphores (sys_sem_t.q holds a SemaphoreHandle_t) ----------------- */
err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
    sem->q = (void *)xSemaphoreCreateCounting(0xFFFFU, count);
    return (sem->q != NULL) ? ERR_OK : ERR_MEM;
}
void sys_sem_free(sys_sem_t *sem) { if (sem->q) { vSemaphoreDelete((SemaphoreHandle_t)sem->q); sem->q = NULL; } }
void sys_sem_signal(sys_sem_t *sem) { (void)xSemaphoreGive((SemaphoreHandle_t)sem->q); }
int  sys_sem_valid(sys_sem_t *sem) { return (sem->q != NULL); }
void sys_sem_set_invalid(sys_sem_t *sem) { sem->q = NULL; }

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout_ms)
{
    u32_t start = sys_now();
    TickType_t ticks = (timeout_ms == 0U) ? portMAX_DELAY : (TickType_t)timeout_ms;
    if (xSemaphoreTake((SemaphoreHandle_t)sem->q, ticks) == pdTRUE)
    {
        return sys_now() - start;
    }
    return SYS_ARCH_TIMEOUT;
}

/* ---- Mutexes ------------------------------------------------------------ */
err_t sys_mutex_new(sys_mutex_t *mtx) { mtx->q = (void *)xSemaphoreCreateMutex(); return (mtx->q != NULL) ? ERR_OK : ERR_MEM; }
void  sys_mutex_free(sys_mutex_t *mtx) { if (mtx->q) { vSemaphoreDelete((SemaphoreHandle_t)mtx->q); mtx->q = NULL; } }
void  sys_mutex_lock(sys_mutex_t *mtx) { (void)xSemaphoreTake((SemaphoreHandle_t)mtx->q, portMAX_DELAY); }
void  sys_mutex_unlock(sys_mutex_t *mtx) { (void)xSemaphoreGive((SemaphoreHandle_t)mtx->q); }
int   sys_mutex_valid(sys_mutex_t *mtx) { return (mtx->q != NULL); }
void  sys_mutex_set_invalid(sys_mutex_t *mtx) { mtx->q = NULL; }

/* ---- Mailboxes (a queue of void* messages) ------------------------------ */
err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
    mbox->q = (void *)xQueueCreate((UBaseType_t)size, sizeof(void *));
    return (mbox->q != NULL) ? ERR_OK : ERR_MEM;
}
void sys_mbox_free(sys_mbox_t *mbox) { if (mbox->q) { vQueueDelete((QueueHandle_t)mbox->q); mbox->q = NULL; } }
void sys_mbox_post(sys_mbox_t *mbox, void *msg) { (void)xQueueSendToBack((QueueHandle_t)mbox->q, &msg, portMAX_DELAY); }
err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg) { return (xQueueSendToBack((QueueHandle_t)mbox->q, &msg, 0U) == pdPASS) ? ERR_OK : ERR_MEM; }
err_t sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg) { return sys_mbox_trypost(mbox, msg); }
int  sys_mbox_valid(sys_mbox_t *mbox) { return (mbox->q != NULL); }
void sys_mbox_set_invalid(sys_mbox_t *mbox) { mbox->q = NULL; }

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout_ms)
{
    u32_t start = sys_now();
    void *rx = NULL;
    TickType_t ticks = (timeout_ms == 0U) ? portMAX_DELAY : (TickType_t)timeout_ms;
    if (xQueueReceive((QueueHandle_t)mbox->q, &rx, ticks) == pdPASS)
    {
        if (msg != NULL) { *msg = rx; }
        return sys_now() - start;
    }
    if (msg != NULL) { *msg = NULL; }
    return SYS_ARCH_TIMEOUT;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
    void *rx = NULL;
    if (xQueueReceive((QueueHandle_t)mbox->q, &rx, 0U) == pdPASS)
    {
        if (msg != NULL) { *msg = rx; }
        return 0U;
    }
    return SYS_MBOX_EMPTY;
}

/* ---- Threads ------------------------------------------------------------ */
sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread,
                            void *arg, int stacksize, int prio)
{
    TaskHandle_t h = NULL;
    (void)xTaskCreate((TaskFunction_t)thread, name,
                      (uint32_t)((stacksize > 0) ? stacksize : (int)configMINIMAL_STACK_SIZE),
                      arg, (UBaseType_t)prio, &h);
    return (sys_thread_t)h;
}

#endif /* !NO_SYS */
