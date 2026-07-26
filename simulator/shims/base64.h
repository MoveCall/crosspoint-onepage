#pragma once
// Minimal stub for Arduino-ESP32's base64 helper. Real encoding/decoding is out of
// scope for the simulator; returns empty strings.

#include <WString.h>

class base64 {
 public:
  static String encode(const uint8_t*, size_t) { return String(); }
  static String encode(const String&) { return String(); }
};
