cmake_minimum_required(VERSION 3.16)
project(arduino_host LANGUAGES CXX VERSION 0.1.0)

# ---------- Options ----------
# FreeRTOS model: currently only `pthread` is implemented (real std::thread under the
# hood — required because consumer code typically does blocking ulTaskNotifyTake /
# xSemaphoreTake calls that single-thread cooperative scheduling cannot honour without
# stackful coroutines). Left as a CMake variable for future-work signalling.
set(ARDUINO_HOST_FREERTOS_MODEL "pthread" CACHE STRING "FreeRTOS implementation model (only 'pthread' supported)")

set(ARDUINO_HOST_HEAP_SIZE_BYTES "393216" CACHE STRING
    "Value returned by ESP.getFreeHeap() etc. Default 384 KiB.")

set(ARDUINO_HOST_SERIAL_TARGET "stderr" CACHE STRING
    "Where Serial writes go by default: stderr (default), stdout, or file (caller must setSink).")
set_property(CACHE ARDUINO_HOST_SERIAL_TARGET PROPERTY STRINGS stderr stdout file)

option(ARDUINO_HOST_BUILD_EXAMPLES "Build arduino-host examples" OFF)
option(ARDUINO_HOST_BUILD_TESTS "Build arduino-host tests" OFF)

# ---------- Library ----------
add_library(arduino_host STATIC
    src/arduino_runtime.cpp
    src/print.cpp
    src/stream.cpp
    src/hardware_serial.cpp
    src/esp_system.cpp
)

target_sources(arduino_host PRIVATE src/freertos_pthread.cpp)
find_package(Threads REQUIRED)
target_link_libraries(arduino_host PUBLIC Threads::Threads)

target_include_directories(arduino_host PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(arduino_host PUBLIC cxx_std_20)

target_compile_definitions(arduino_host PRIVATE
    ARDUINO_HOST_HEAP_SIZE_BYTES=${ARDUINO_HOST_HEAP_SIZE_BYTES}
)
if(ARDUINO_HOST_SERIAL_TARGET STREQUAL "stdout")
    target_compile_definitions(arduino_host PRIVATE ARDUINO_HOST_SERIAL_STDOUT=1)
elseif(ARDUINO_HOST_SERIAL_TARGET STREQUAL "file")
    target_compile_definitions(arduino_host PRIVATE ARDUINO_HOST_SERIAL_FILE=1)
endif()

# Match the firmware "no exceptions" contract. RTTI is intentionally kept enabled on
# host: libc++ (macOS) needs typeid in its std::function converting constructor's
# SFINAE check, and host binaries don't care about the few KB RTTI tables cost.
target_compile_options(arduino_host PUBLIC -fno-exceptions)

# ---------- Examples ----------
if(ARDUINO_HOST_BUILD_EXAMPLES)
    add_subdirectory(examples/blink_headless)
endif()

# ---------- Tests ----------
if(ARDUINO_HOST_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
