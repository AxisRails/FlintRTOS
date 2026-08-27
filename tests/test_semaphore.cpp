// FlintRTOS - tests for semaphores/mutex over the queue (manifest 1).
#include "test_framework.hpp"
extern "C" {
#include "semphr.h"
}

FLINT_TEST(sem_binary_give_take) {
    SemaphoreHandle_t s = xSemaphoreCreateBinary();
    CHECK(s != nullptr);
    CHECK(xSemaphoreTake(s, 0) == pdFALSE);   // starts empty
    CHECK(xSemaphoreGive(s) == pdPASS);
    CHECK(uxSemaphoreGetCount(s) == 1U);
    CHECK(xSemaphoreTake(s, 0) == pdTRUE);
    CHECK(xSemaphoreTake(s, 0) == pdFALSE);   // empty again
    vSemaphoreDelete(s);
}

FLINT_TEST(sem_counting) {
    SemaphoreHandle_t s = xSemaphoreCreateCounting(3, 2);
    CHECK(s != nullptr);
    CHECK(uxSemaphoreGetCount(s) == 2U);
    CHECK(xSemaphoreTake(s, 0) == pdTRUE);
    CHECK(xSemaphoreTake(s, 0) == pdTRUE);
    CHECK(xSemaphoreTake(s, 0) == pdFALSE);   // exhausted
    CHECK(xSemaphoreGive(s) == pdPASS);
    CHECK(uxSemaphoreGetCount(s) == 1U);
    vSemaphoreDelete(s);
}

FLINT_TEST(mutex_starts_available) {
    SemaphoreHandle_t m = xSemaphoreCreateMutex();
    CHECK(m != nullptr);
    CHECK(xSemaphoreTake(m, 0) == pdTRUE);    // available -> take succeeds
    CHECK(xSemaphoreTake(m, 0) == pdFALSE);   // now held
    CHECK(xSemaphoreGive(m) == pdPASS);       // release
    CHECK(xSemaphoreTake(m, 0) == pdTRUE);
    vSemaphoreDelete(m);
}
