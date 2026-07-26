# arduino-host

A project-independent host-side runtime for Arduino / FreeRTOS / ESP32 firmware. Lets you
build and run code targeting ESP32 (Arduino framework) on a development machine — Linux,
macOS, CI runners, Docker — without flashing a real device.

This is a **runtime**, not an instruction-set emulator. It provides working implementations
of the Arduino, FreeRTOS, and ESP system APIs that firmware code calls into, but does not
emulate the chip's CPU or peripherals. The intended use case is verifying application logic,
running unit tests, and powering visual simulators (paired with a project-specific HAL
backend that draws to SDL / Qt / a terminal / whatever).

## What's included

This layer covers Arduino *core* + FreeRTOS + ESP-IDF system APIs. Anything beyond core
(WiFi, esp_http_client, OTA, web/socket servers, image decoders, QR…) is **out of scope**
and must be supplied by the consumer project as its own shim or real implementation.

- **Arduino core**: `millis()`, `micros()`, `delay()`, `delayMicroseconds()`, `yield()`,
  `String`, `Print`, `Stream`, `HardwareSerial` (writes to stdout/stderr/file),
  `random()`, `map()`, `constrain()`. `pinMode` / `digitalWrite` / `digitalRead` /
  `analogRead` / `analogWrite` are declared as weak no-ops so they link cleanly even on
  host; consumers can override with strong symbols.
- **Arduino core buses**: `SPI` (`SPIClass`) and `Wire` (`TwoWire`) — header stubs with
  empty default methods + a global instance. Override with your own implementation if a
  project needs realistic SPI/I2C behaviour.
- **FreeRTOS**: `xTaskCreate`, `vTaskDelete`, `vTaskDelay`, task notifications, semaphores
  (mutex / recursive / binary / counting), queues, critical sections. Implemented over
  `std::thread` + `std::mutex` + `std::condition_variable`, so blocking primitives
  (`ulTaskNotifyTake(portMAX_DELAY)` and friends) behave as on real FreeRTOS.
- **ESP system**: `ESP.getFreeHeap()`, `ESP.restart()`, `esp_reset_reason()`,
  `setCpuFrequencyMhz()`, `esp_sleep_*()` enums and stubs, `esp_random()` /
  `esp_fill_random()` (forwards to thread-local `std::mt19937`, **not** cryptographic).
  `RTC_NOINIT_ATTR` and `RTC_DATA_ATTR` resolve to nothing.

## What's NOT included

- **Networking / OTA**: WiFi, esp_http_client (+ esp_crt_bundle), NetworkUdp, WebServer,
  WebSocketsServer, esp_ota_ops, MD5Builder. Provide your own shim or real
  implementation.
- **Third-party libraries**: image codecs (PNGdec, JPEGDEC), QR code generators
  (`qrcode.h`), base64 helpers. Same — out of scope.
- **GUI / windowing**: arduino-host has zero SDL/Qt/X11 dependency. Pair with your own
  presenter (SDL2, terminal, web canvas, …).
- **Specific peripheral devices**: displays, sensors, SD cards, IMUs. Those are the
  consumer project's HAL responsibility; arduino-host only provides the bus / runtime
  surface they call into.
- **ISA emulation**: this is not QEMU — code runs natively as host machine code.

## Integration

```cmake
add_subdirectory(path/to/arduino-host)
target_link_libraries(my_simulator PRIVATE arduino_host)
```

Or via `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(arduino_host GIT_REPOSITORY ... GIT_TAG ...)
FetchContent_MakeAvailable(arduino_host)
target_link_libraries(my_simulator PRIVATE arduino_host)
```

Your project supplies its own `int main()` — typically driving `setup()` and `loop()`
explicitly:

```cpp
#include <Arduino.h>

void setup();
void loop();

int main() {
  setup();
  while (true) loop();
}
```

## CMake options

| Option                          | Default     | Effect |
|---------------------------------|-------------|--------|
| `ARDUINO_HOST_FREERTOS_MODEL`   | `pthread`   | Only `pthread` is currently implemented. `xTaskCreate` spawns real `std::thread`s; blocking primitives use `std::condition_variable`. A future cooperative single-thread mode would need stackful coroutines. |
| `ARDUINO_HOST_HEAP_SIZE_BYTES`  | `393216`    | Value returned by `ESP.getFreeHeap()`. Set lower to test low-memory paths. |
| `ARDUINO_HOST_SERIAL_TARGET`    | `stderr`    | Where `Serial` writes by default: `stderr`, `stdout`, or `file` (caller must call `setSink()`). |
| `ARDUINO_HOST_BUILD_EXAMPLES`   | `OFF`       | Build `blink_headless` example. |
| `ARDUINO_HOST_BUILD_TESTS`      | `OFF`       | Build `test_runtime` and register with CTest. |

## Runtime hooks

```cpp
#include <Arduino.h>

arduino_host::set_restart_handler([] { /* what to do on ESP.restart() */ });
arduino_host::set_free_heap_bytes(64 * 1024);          // simulate 64 KiB headroom
arduino_host::set_serial_sink(some_file_or_stdout);
```

## Verify standalone

```sh
cmake -S . -B build -DARDUINO_HOST_BUILD_EXAMPLES=ON -DARDUINO_HOST_BUILD_TESTS=ON
cmake --build build -j
./build/examples/blink_headless/blink_headless
ctest --test-dir build --output-on-failure
```

The example prints `[t=… ms] LED=ON/OFF` lines and exits after 5 iterations. The test binary
exercises millis monotonicity, String semantics, semaphore take/give, and a task-notify
round trip.

## Caveats

- `xTaskCreate` uses real OS threads. Application code that assumes single-core ordering
  (e.g. relies on the absence of preemption) may behave differently than on a real
  single-core ESP32-C3. Use mutexes / atomics as you would on the chip — your code is
  already FreeRTOS-correct if it ran there.
- `uxTaskGetStackHighWaterMark()` returns a constant (4096). Stack accounting is
  meaningless on host since threads use OS stacks.
- `taskENTER_CRITICAL` uses a single global recursive mutex. It serializes correctly but
  does not match the per-CPU spinlock semantics of real ESP32 FreeRTOS.
- The Arduino `String` class is implemented as a wrapper over `std::string`. Memory
  allocation strategy differs (no SSO/no shared buffer); behaviour matches semantically.
- `-fno-rtti` is **not** propagated (only `-fno-exceptions` is). libc++'s `std::function`
  converting constructor uses `typeid` in its SFINAE check; with RTTI disabled it
  silently rejects valid lambdas. Host binaries don't care about the few KB the RTTI
  tables cost, and consumer firmware code is unaffected because it links against
  GCC libstdc++ on the device.

## License

Same license as the parent project that vendors this directory.
