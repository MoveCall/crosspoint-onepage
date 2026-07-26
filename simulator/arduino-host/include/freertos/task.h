#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

struct TaskState_;
using TaskHandle_t = TaskState_*;
using TaskFunction_t = void (*)(void*);

enum eNotifyAction {
  eNoAction = 0,
  eSetBits,
  eIncrement,
  eSetValueWithOverwrite,
  eSetValueWithoutOverwrite,
};

BaseType_t xTaskCreate(TaskFunction_t pvTaskCode, const char* pcName, uint32_t usStackDepth, void* pvParameters,
                       UBaseType_t uxPriority, TaskHandle_t* pxCreatedTask);

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t pvTaskCode, const char* pcName, uint32_t usStackDepth,
                                   void* pvParameters, UBaseType_t uxPriority, TaskHandle_t* pxCreatedTask,
                                   BaseType_t xCoreID);

void vTaskDelete(TaskHandle_t xTaskToDelete);
void vTaskDelay(const TickType_t xTicksToDelay);
void vTaskSuspend(TaskHandle_t xTaskToSuspend);
void vTaskResume(TaskHandle_t xTaskToResume);

TaskHandle_t xTaskGetCurrentTaskHandle();
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t xTask);

BaseType_t xTaskNotify(TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction);
BaseType_t xTaskNotifyGive(TaskHandle_t xTaskToNotify);
uint32_t ulTaskNotifyTake(BaseType_t xClearCountOnExit, TickType_t xTicksToWait);
BaseType_t xTaskNotifyWait(uint32_t ulBitsToClearOnEntry, uint32_t ulBitsToClearOnExit, uint32_t* pulNotificationValue,
                           TickType_t xTicksToWait);
