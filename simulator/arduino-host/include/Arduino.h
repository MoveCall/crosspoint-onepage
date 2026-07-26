#pragma once

#include <HardwareSerial.h>
#include <Print.h>
#include <SPI.h>
#include <Stream.h>
#include <WString.h>
#include <Wire.h>
#include <esp32-hal-cpu.h>
#include <esp32-hal-gpio.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>

// Timing
unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned long us);

// Arduino's cooperative yield hint. On host, yield to the OS scheduler.
void yield();

// Randomness
long random(long max);
long random(long min, long max);
void randomSeed(unsigned long seed);

// Math helpers (function templates to keep `min(int,size_t)` style calls working)
template <typename T>
constexpr T constrain(T val, T lo, T hi) {
  return val < lo ? lo : (val > hi ? hi : val);
}

template <typename T>
constexpr T map(T x, T inMin, T inMax, T outMin, T outMax) {
  return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

// NOTE: Arduino traditionally provides `min` and `max` as preprocessor macros.
// They wreck `std::min` / `std::max` call sites, so we intentionally do NOT define
// them. Use `std::min` / `std::max` (or write a local helper) on host.

// Pin direction macros (compat with esp32-hal-gpio.h definitions)
#ifndef HIGH
#define HIGH 0x1
#endif
#ifndef LOW
#define LOW 0x0
#endif

// Boards use these in places we don't care about on host.
using boolean = bool;
using byte = uint8_t;
using word = uint16_t;

// Arduino-host extension API: hooks consumers can use to customise behaviour.
namespace arduino_host {
using RestartHandler = void (*)();
void set_restart_handler(RestartHandler fn);
void set_free_heap_bytes(size_t bytes);
void set_serial_sink(std::FILE* sink);
}  // namespace arduino_host
