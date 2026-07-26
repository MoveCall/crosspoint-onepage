// Smoke tests for arduino-host primitives. Uses plain asserts so the binary fails
// fast under CTest without pulling in a test framework.

#include <Arduino.h>

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>

namespace {

void test_millis_monotonic() {
  unsigned long a = millis();
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  unsigned long b = millis();
  assert(b >= a + 15);  // Allow some slack for sleep accuracy.
}

void test_string_basics() {
  String s = "hello world";
  assert(s.length() == 11);
  assert(s.startsWith("hello"));
  assert(s.endsWith("world"));
  assert(s.indexOf(' ') == 5);
  assert(s.substring(6) == "world");
  s.toLowerCase();
  assert(s == "hello world");
  s += "!";
  assert(s.endsWith("!"));
}

void test_freertos_semaphore() {
  SemaphoreHandle_t mtx = xSemaphoreCreateMutex();
  assert(mtx != nullptr);
  assert(xSemaphoreTake(mtx, 0) == pdPASS);
  assert(xSemaphoreGive(mtx) == pdPASS);
  vSemaphoreDelete(mtx);
}

std::atomic<bool> g_task_ran{false};
TaskHandle_t g_main = nullptr;

void demo_task(void*) {
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  g_task_ran = true;
  xTaskNotify(g_main, 1, eIncrement);
}

void test_freertos_task_notify_round_trip() {
  g_main = xTaskGetCurrentTaskHandle();
  // Main thread needs a TaskHandle_t for xTaskNotify; we install one by registering
  // ourselves as a synthetic task. In our model only threads spawned via xTaskCreate
  // own a TaskState_, so we use a helper task to *send* notifications.
  TaskHandle_t worker = nullptr;
  xTaskCreate(&demo_task, "demo", 4096, nullptr, 1, &worker);

  // Let the worker reach ulTaskNotifyTake before we notify.
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  xTaskNotifyGive(worker);

  // Wait for the worker to set the flag (proves the round-trip happened).
  unsigned long deadline = millis() + 1000;
  while (!g_task_ran && millis() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  assert(g_task_ran);
  vTaskDelete(worker);
}

void test_esp_class() {
  uint32_t heap = ESP.getFreeHeap();
  assert(heap > 0);
  arduino_host::set_free_heap_bytes(123456);
  assert(ESP.getFreeHeap() == 123456);
}

}  // namespace

int main() {
  test_millis_monotonic();
  std::printf("[ok] millis monotonic\n");
  test_string_basics();
  std::printf("[ok] String basics\n");
  test_freertos_semaphore();
  std::printf("[ok] FreeRTOS semaphore\n");
  test_freertos_task_notify_round_trip();
  std::printf("[ok] FreeRTOS task + notify round trip\n");
  test_esp_class();
  std::printf("[ok] ESP class + heap hook\n");
  std::printf("all arduino-host runtime tests passed\n");
  return 0;
}
