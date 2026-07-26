#include <Arduino.h>
#include <esp_random.h>
#include <esp_system.h>

#include <cstdlib>
#include <random>

namespace {
#ifndef ARDUINO_HOST_HEAP_SIZE_BYTES
#define ARDUINO_HOST_HEAP_SIZE_BYTES (384 * 1024)
#endif

size_t g_free_heap = ARDUINO_HOST_HEAP_SIZE_BYTES;
arduino_host::RestartHandler g_restart = nullptr;
}  // namespace

uint32_t EspClass::getFreeHeap() const { return static_cast<uint32_t>(g_free_heap); }
uint32_t EspClass::getMinFreeHeap() const { return static_cast<uint32_t>(g_free_heap); }
uint32_t EspClass::getMaxAllocHeap() const { return static_cast<uint32_t>(g_free_heap); }
uint32_t EspClass::getHeapSize() const { return static_cast<uint32_t>(ARDUINO_HOST_HEAP_SIZE_BYTES); }

void EspClass::restart() {
  if (g_restart) {
    g_restart();
  } else {
    std::exit(0);
  }
}

EspClass ESP;

void esp_restart() { ESP.restart(); }

esp_reset_reason_t esp_reset_reason() { return ESP_RST_POWERON; }

esp_sleep_source_t esp_sleep_get_wakeup_cause() { return ESP_SLEEP_WAKEUP_UNDEFINED; }
int esp_sleep_config_gpio_isolate() { return 0; }

namespace arduino_host {
void set_restart_handler(RestartHandler fn) { g_restart = fn; }
void set_free_heap_bytes(size_t bytes) { g_free_heap = bytes; }
}  // namespace arduino_host

namespace {
std::mt19937& esp_rng() {
  thread_local std::mt19937 engine{std::random_device{}()};
  return engine;
}
}  // namespace

uint32_t esp_random() { return static_cast<uint32_t>(esp_rng()()); }

void esp_fill_random(void* buf, size_t len) {
  auto* p = static_cast<uint8_t*>(buf);
  for (size_t i = 0; i < len; i++) p[i] = static_cast<uint8_t>(esp_random() & 0xFF);
}
