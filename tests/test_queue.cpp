// FlintRTOS - tests for Queue Management (manifest 1) core FIFO semantics.
#include "test_framework.hpp"

extern "C" {
#include "queue.h"
}

FLINT_TEST(queue_create_and_counts) {
    QueueHandle_t q = xQueueCreate(4, sizeof(uint32_t));
    CHECK(q != nullptr);
    CHECK(uxQueueMessagesWaiting(q) == 0U);
    CHECK(uxQueueSpacesAvailable(q) == 4U);
    vQueueDelete(q);
}

FLINT_TEST(queue_fifo_order) {
    QueueHandle_t q = xQueueCreate(4, sizeof(uint32_t));
    for (uint32_t i = 1; i <= 3; ++i) {
        CHECK(xQueueSendToBack(q, &i, 0) == pdPASS);
    }
    CHECK(uxQueueMessagesWaiting(q) == 3U);
    for (uint32_t expect = 1; expect <= 3; ++expect) {
        uint32_t got = 0;
        CHECK(xQueueReceive(q, &got, 0) == pdPASS);
        CHECK(got == expect);
    }
    CHECK(uxQueueMessagesWaiting(q) == 0U);
    vQueueDelete(q);
}

FLINT_TEST(queue_full_and_empty_guards) {
    QueueHandle_t q = xQueueCreate(2, sizeof(uint32_t));
    uint32_t v = 7;
    CHECK(xQueueSendToBack(q, &v, 0) == pdPASS);
    CHECK(xQueueSendToBack(q, &v, 0) == pdPASS);
    CHECK(xQueueSendToBack(q, &v, 0) == pdFAIL);   // full
    uint32_t got = 0;
    CHECK(xQueueReceive(q, &got, 0) == pdPASS);
    CHECK(xQueueReceive(q, &got, 0) == pdPASS);
    CHECK(xQueueReceive(q, &got, 0) == pdFAIL);     // empty
    vQueueDelete(q);
}

FLINT_TEST(queue_send_to_front_is_lifo_at_head) {
    QueueHandle_t q = xQueueCreate(4, sizeof(uint32_t));
    uint32_t a = 10, b = 20;
    CHECK(xQueueSendToBack(q, &a, 0) == pdPASS);   // [10]
    CHECK(xQueueSendToFront(q, &b, 0) == pdPASS);  // [20,10]
    uint32_t got = 0;
    CHECK(xQueueReceive(q, &got, 0) == pdPASS);
    CHECK(got == 20U);                              // front item first
    CHECK(xQueueReceive(q, &got, 0) == pdPASS);
    CHECK(got == 10U);
    vQueueDelete(q);
}

FLINT_TEST(queue_wraparound) {
    // Exercise circular wrap: fill, drain, refill past the end.
    QueueHandle_t q = xQueueCreate(3, sizeof(uint32_t));
    uint32_t v, got;
    for (uint32_t round = 0; round < 3; ++round) {
        for (uint32_t i = 0; i < 3; ++i) { v = round * 10 + i; CHECK(xQueueSendToBack(q, &v, 0) == pdPASS); }
        for (uint32_t i = 0; i < 3; ++i) { got = 99; CHECK(xQueueReceive(q, &got, 0) == pdPASS); CHECK(got == round * 10 + i); }
    }
    vQueueDelete(q);
}
