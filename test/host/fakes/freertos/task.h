#ifndef FAKE_FREERTOS_TASK_H
#define FAKE_FREERTOS_TASK_H
#include "FreeRTOS.h"
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn, const char* name, uint32_t stack, void* arg,
                                   UBaseType_t priority, TaskHandle_t* handle, BaseType_t core);
uint32_t ulTaskNotifyTake(BaseType_t clearOnExit, TickType_t wait);
BaseType_t xTaskNotifyGive(TaskHandle_t task);
#endif
