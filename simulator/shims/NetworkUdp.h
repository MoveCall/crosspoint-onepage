#pragma once
// Header-only stub: type-only presence so WiFi-aware project headers parse.

#include <WiFi.h>

#include <cstdint>

class NetworkUDP {
 public:
  bool begin(uint16_t) { return false; }
  void stop() {}
  int parsePacket() { return 0; }
  int read(uint8_t*, size_t) { return 0; }
  IPAddress remoteIP() { return IPAddress(); }
  uint16_t remotePort() { return 0; }
  void beginPacket(IPAddress, uint16_t) {}
  void beginPacket(const char*, uint16_t) {}
  size_t write(const uint8_t*, size_t) { return 0; }
  int endPacket() { return 0; }
};

using WiFiUDP = NetworkUDP;
