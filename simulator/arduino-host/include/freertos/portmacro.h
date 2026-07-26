#pragma once

#include <freertos/FreeRTOS.h>

using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0

void taskENTER_CRITICAL(portMUX_TYPE* mux);
void taskEXIT_CRITICAL(portMUX_TYPE* mux);

#define portENTER_CRITICAL(mux) taskENTER_CRITICAL(mux)
#define portEXIT_CRITICAL(mux) taskEXIT_CRITICAL(mux)
#define portENTER_CRITICAL_ISR(mux) taskENTER_CRITICAL(mux)
#define portEXIT_CRITICAL_ISR(mux) taskEXIT_CRITICAL(mux)

#define portYIELD_FROM_ISR(x) ((void)0)
