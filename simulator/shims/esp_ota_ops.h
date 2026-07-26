#pragma once
// Stub for ESP-IDF OTA API. OTA is out of scope on host; this header provides the
// type shapes so consumer headers parse, but no .cpp that calls these should be
// compiled by the simulator.

#include <cstddef>
#include <cstdint>

typedef struct esp_partition_t {
  uint32_t address;
  uint32_t size;
  const char* label;
} esp_partition_t;

typedef void* esp_ota_handle_t;

const esp_partition_t* esp_ota_get_running_partition();
const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t* start_from);
int esp_ota_begin(const esp_partition_t* partition, size_t image_size, esp_ota_handle_t* out_handle);
int esp_ota_write(esp_ota_handle_t handle, const void* data, size_t size);
int esp_ota_end(esp_ota_handle_t handle);
int esp_ota_set_boot_partition(const esp_partition_t* partition);
