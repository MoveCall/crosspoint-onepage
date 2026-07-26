#pragma once

#include <freertos/FreeRTOS.h>

struct Queue_;
using QueueHandle_t = Queue_*;

QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize);
void vQueueDelete(QueueHandle_t xQueue);

BaseType_t xQueueSend(QueueHandle_t xQueue, const void* pvItemToQueue, TickType_t xTicksToWait);
BaseType_t xQueueSendFromISR(QueueHandle_t xQueue, const void* pvItemToQueue, BaseType_t* pxHigherPriorityTaskWoken);
BaseType_t xQueueReceive(QueueHandle_t xQueue, void* pvBuffer, TickType_t xTicksToWait);
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t xQueue);
BaseType_t xQueuePeek(QueueHandle_t xQueue, void* pvBuffer, TickType_t xTicksToWait);

// On real FreeRTOS the queue and semaphore handle types are unified, so xQueuePeek
// also accepts a mutex handle. Provide an overload that returns pdTRUE when the mutex
// is available (not held), pdFALSE if held.
struct Semaphore_;
BaseType_t xQueuePeek(struct Semaphore_* xSemaphore, void* pvBuffer, TickType_t xTicksToWait);
