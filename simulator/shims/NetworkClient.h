#pragma once
// WiFiClient + NetworkClient shim for the simulator. Modern Arduino-ESP32 exposes the
// plain (non-TLS) socket as NetworkClient (an alias of WiFiClient). The simulator no
// longer drives any HTTP path through this type — HttpDownloader uses
// esp_http_client.h directly — so the class exists only for PubSubClient, which holds
// a WiFiClient by value for MQTT TCP. Methods return false / -1 so MQTT connection
// attempts fail gracefully in both native (libcurl is HTTP-only) and WASM (offline).

#include <cstddef>
#include <cstdint>

class WiFiClient {
 public:
  virtual ~WiFiClient() = default;
  bool connect(const char*, uint16_t) { return false; }
  void stop() {}
  bool connected() { return false; }
  int available() { return 0; }
  int read() { return -1; }
  size_t write(const uint8_t*, size_t) { return 0; }
};

using NetworkClient = WiFiClient;
