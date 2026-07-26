#pragma once
// Minimal ESP-IDF error code shim. Mirrors the subset of esp_err.h that
// HttpDownloader and other shims need: the type, the ESP_OK / ESP_FAIL
// sentinels, and esp_err_to_name for log lines.

#ifdef __cplusplus
extern "C" {
#endif

typedef int esp_err_t;

#define ESP_OK 0
#define ESP_FAIL -1

inline const char* esp_err_to_name(esp_err_t err) {
  switch (err) {
    case ESP_OK:
      return "ESP_OK";
    case ESP_FAIL:
      return "ESP_FAIL";
    default:
      return "ESP_ERR";
  }
}

#ifdef __cplusplus
}
#endif
