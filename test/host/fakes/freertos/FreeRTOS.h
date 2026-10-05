#ifndef FAKE_FREERTOS_H
#define FAKE_FREERTOS_H

// Tasks do not run on their own: fake::runTasks() runs each created task until it waits for a
// notification that has not been given. Single threaded, so the mutexes are no-ops.

#include <cstdint>

typedef int BaseType_t;
typedef unsigned UBaseType_t;
typedef uint32_t TickType_t;
typedef void (*TaskFunction_t)(void*);
typedef struct FakeTask* TaskHandle_t;
typedef struct FakeSemaphore* SemaphoreHandle_t;

#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define portMAX_DELAY 0xffffffffu

#endif
