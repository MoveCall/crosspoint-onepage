#pragma once

#include <cstddef>
#include <cstdint>

class SPIClass {
 public:
  void begin(int8_t sck = -1, int8_t miso = -1, int8_t mosi = -1, int8_t cs = -1) {}
  void end() {}
  void setFrequency(uint32_t hz) {}
  uint8_t transfer(uint8_t data) { return 0; }
  uint16_t transfer16(uint16_t data) { return 0; }
  void transfer(void* buf, size_t count) {}
};

extern SPIClass SPI;
