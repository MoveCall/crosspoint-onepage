#pragma once
// Host stub for ESP-IDF SNTP. The simulator has no Wi-Fi path; consumers (StandbyActivity)
// gate the SNTP calls behind a "already synced" flag in CROSSPOINT_EMULATED builds, so
// these stubs exist only to make the firmware sources compile and link on the host.

#include <cstdint>

typedef enum {
  SNTP_SYNC_STATUS_RESET = 0,
  SNTP_SYNC_STATUS_COMPLETED,
  SNTP_SYNC_STATUS_IN_PROGRESS,
} sntp_sync_status_t;

inline bool esp_sntp_enabled() { return false; }
inline void esp_sntp_stop() {}
inline sntp_sync_status_t sntp_get_sync_status() { return SNTP_SYNC_STATUS_RESET; }
inline void configTime(long, int, const char*, const char* = nullptr, const char* = nullptr) {}
