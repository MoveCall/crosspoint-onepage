#pragma once

#include <cstddef>
#include <cstdint>

// ESP-IDF hardware RNG surface. On real ESP32 these draw from a TRNG. The host
// implementation forwards to a thread-local std::mt19937 seeded from std::random_device
// — adequate for procedural art / sample IDs / non-cryptographic use, not for keys.

uint32_t esp_random();
void esp_fill_random(void* buf, size_t len);
