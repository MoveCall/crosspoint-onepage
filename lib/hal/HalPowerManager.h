#pragma once

#include <Arduino.h>
#include <BatteryMonitor.h>
#include <InputManager.h>
#include <Logging.h>
#include <Wire.h>
#include <freertos/semphr.h>

#include <cassert>

#include "HalGPIO.h"

class HalPowerManager;
extern HalPowerManager powerManager;  // Singleton

class HalPowerManager {
  int normalFreq = 0;      // MHz
  int currentSetFreq = 0;  // last frequency we set (MHz); 0 = unknown/uninitialized

  // I2C fuel gauge configuration for X3 battery monitoring
  bool _batteryUseI2C = false;                   // True if using I2C fuel gauge (X3), false for ADC (X4)
  mutable int _batteryCachedPercent = 0;         // Last read battery percentage (0-100)
  mutable unsigned long _batteryLastPollMs = 0;  // Timestamp of last battery read in milliseconds

  enum LockMode { None, NormalSpeed };
  LockMode currentLockMode = None;
  SemaphoreHandle_t modeMutex = nullptr;  // Protect access to currentLockMode

  static volatile bool bleActive_;  // true while a BLE page-turner link is up

 public:
  static constexpr int LOW_POWER_FREQ = 10;                    // MHz
  static constexpr int BLE_ACTIVE_FREQ = 80;                   // MHz (see setBleActive)
  static constexpr unsigned long IDLE_POWER_SAVING_MS = 2000;  // ms
  static constexpr unsigned long BATTERY_POLL_MS = 1500;       // ms

  void begin();

  // Control CPU frequency for power saving
  void setPowerSaving(bool enabled);

  // BLE page-turner coexistence (ONEPAGE_C61): the BLE controller's radio timing
  // is calibrated for the CPU clock; dropping to LOW_POWER_FREQ (10 MHz) while a
  // BLE link is up corrupts controller timing and trips the interrupt watchdog
  // (Interrupt wdt timeout on CPU0 -> reboot). Instead of holding full speed
  // (160 MHz, the old behavior), we clamp to BLE_ACTIVE_FREQ (80 MHz) while a
  // link is up: still comfortably above the radio's timing floor, but ~half the
  // CPU dynamic power. Set by the BLE host on start()/stop(); read in
  // setPowerSaving().
  static void setBleActive(bool active) { bleActive_ = active; }
  static bool isBleActive() { return bleActive_; }

  // Setup wake up GPIO and enter deep sleep
  // Should be called inside main loop() to handle the currentLockMode
  void startDeepSleep(HalGPIO& gpio) const;

  // Get battery percentage (range 0-100)
  uint16_t getBatteryPercentage() const;

  // RAII helper class to manage power saving locks
  // Usage: create an instance of Lock in a scope to disable power saving, for example when running a task that needs
  // full performance. When the Lock instance is destroyed (goes out of scope), power saving will be re-enabled.
  class Lock {
    friend class HalPowerManager;
    bool valid = false;

   public:
    explicit Lock();
    ~Lock();

    // Non-copyable and non-movable
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
    Lock(Lock&&) = delete;
    Lock& operator=(Lock&&) = delete;
  };
};
