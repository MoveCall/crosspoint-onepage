#pragma once
// Stub for ESP-IDF CA bundle helper. On device, esp_http_client uses this to
// attach the Mozilla root bundle for TLS verification. The simulator delegates
// TLS to libcurl (native) or to no-op stubs (WASM), so this is just a non-null
// sentinel that esp_http_client_config_t can carry without changing the device
// call sites.

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_tls esp_tls_t;

// Signature matches ESP-IDF: int (*)(void* conf). Simulator never invokes it.
inline int esp_crt_bundle_attach(void* /*conf*/) { return 0; }

#ifdef __cplusplus
}
#endif
