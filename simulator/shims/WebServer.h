#pragma once
// Header stub: lets WebServer-aware headers parse on host. WebServer functionality
// is out of scope for the simulator; consumers shouldn't compile against this stub.

#include <WString.h>

#include <cstdint>
#include <functional>

class WebServer {
 public:
  using THandlerFunction = std::function<void()>;
  explicit WebServer(int = 80) {}
  void begin() {}
  void stop() {}
  void handleClient() {}
  void on(const char*, THandlerFunction) {}
  String arg(const char*) { return String(); }
  bool hasArg(const char*) { return false; }
  void send(int, const char*, const String& = String()) {}
};
