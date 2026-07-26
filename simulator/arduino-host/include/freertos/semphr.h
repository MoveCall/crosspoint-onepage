#pragma once

#include <freertos/FreeRTOS.h>

struct Semaphore_;
using SemaphoreHandle_t = Semaphore_*;

SemaphoreHandle_t xSemaphoreCreateMutex();
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex();
SemaphoreHandle_t xSemaphoreCreateBinary();
SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t uxMaxCount, UBaseType_t uxInitialCount);

void vSemaphoreDelete(SemaphoreHandle_t xSemaphore);

BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait);
BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore);
BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait);
BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t xSemaphore);

// ISR variants: on host there's no ISR context, treat as plain calls.
BaseType_t xSemaphoreTakeFromISR(SemaphoreHandle_t xSemaphore, BaseType_t* pxHigherPriorityTaskWoken);
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t xSemaphore, BaseType_t* pxHigherPriorityTaskWoken);

// Returns the handle of the task currently holding the mutex, or nullptr if not held.
struct TaskState_;
TaskState_* xSemaphoreGetMutexHolder(SemaphoreHandle_t xSemaphore);
