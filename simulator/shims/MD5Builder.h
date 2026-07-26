#pragma once
// Stub for Arduino-ESP32's MD5Builder. Provides the API shape but does not actually
// compute MD5 — fine for compile-time, but consumers that need real MD5 must replace
// the symbol or avoid the host path.

#include <WString.h>

#include <cstdint>
#include <cstring>

class MD5Builder {
 public:
  void begin() { std::memset(buf_, 0, sizeof(buf_)); }
  void add(const uint8_t*, size_t) {}
  void add(const char*) {}
  void add(const String&) {}
  void addStream(class Stream&, size_t) {}
  void calculate() {}
  String toString() { return String("00000000000000000000000000000000"); }
  void getBytes(uint8_t* out) { std::memset(out, 0, 16); }

 private:
  uint8_t buf_[16] = {};
};
