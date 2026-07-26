#pragma once

#include <cstddef>
#include <cstdint>

class TwoWire {
 public:
  bool begin(int sda = -1, int scl = -1, uint32_t frequency = 0) { return true; }
  bool end() { return true; }
  void setClock(uint32_t hz) {}
  void beginTransmission(uint8_t address) {}
  uint8_t endTransmission(bool stop = true) { return 0; }
  size_t write(uint8_t b) { return 1; }
  size_t write(const uint8_t* data, size_t n) { return n; }
  uint8_t requestFrom(uint8_t address, uint8_t quantity, bool stop = true) { return 0; }
  int available() { return 0; }
  int read() { return -1; }
};

extern TwoWire Wire;
