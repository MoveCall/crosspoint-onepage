#pragma once

#include <esp_random.h>  // Real Arduino-ESP32 chain pulls this in transitively; mirror that.

#include <cstddef>
#include <cstdint>

class EspClass {
 public:
  uint32_t getFreeHeap() const;
  uint32_t getMinFreeHeap() const;
  uint32_t getMaxAllocHeap() const;
  uint32_t getHeapSize() const;
  void restart();
};

extern EspClass ESP;

typedef enum {
  ESP_RST_UNKNOWN = 0,
  ESP_RST_POWERON,
  ESP_RST_EXT,
  ESP_RST_SW,
  ESP_RST_PANIC,
  ESP_RST_INT_WDT,
  ESP_RST_TASK_WDT,
  ESP_RST_WDT,
  ESP_RST_DEEPSLEEP,
  ESP_RST_BROWNOUT,
  ESP_RST_SDIO,
} esp_reset_reason_t;

esp_reset_reason_t esp_reset_reason();
void esp_restart();

// Sleep API stubs — preserve signatures, no-op behaviour.
typedef enum {
  ESP_SLEEP_WAKEUP_UNDEFINED = 0,
  ESP_SLEEP_WAKEUP_ALL,
  ESP_SLEEP_WAKEUP_EXT0,
  ESP_SLEEP_WAKEUP_EXT1,
  ESP_SLEEP_WAKEUP_TIMER,
  ESP_SLEEP_WAKEUP_TOUCHPAD,
  ESP_SLEEP_WAKEUP_ULP,
  ESP_SLEEP_WAKEUP_GPIO,
  ESP_SLEEP_WAKEUP_UART,
} esp_sleep_source_t;

esp_sleep_source_t esp_sleep_get_wakeup_cause();
int esp_sleep_config_gpio_isolate();

// ESP-IDF data placement attributes — no meaning on host, expand to nothing.
#ifndef RTC_NOINIT_ATTR
#define RTC_NOINIT_ATTR
#endif
#ifndef RTC_DATA_ATTR
#define RTC_DATA_ATTR
#endif
#ifndef RTC_RODATA_ATTR
#define RTC_RODATA_ATTR
#endif
