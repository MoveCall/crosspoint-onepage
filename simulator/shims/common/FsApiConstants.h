#pragma once

// Shim of SdFat's common/FsApiConstants.h — just the open-flag constants that
// lib/hal/HalStorage.h's signatures reference.

#include <cstdint>

typedef uint8_t oflag_t;

#ifndef O_RDONLY
#define O_RDONLY 0x00
#define O_WRONLY 0x01
#define O_RDWR 0x02
#define O_AT_END 0x04
#define O_APPEND 0x08
#define O_CREAT 0x10
#define O_TRUNC 0x20
#define O_EXCL 0x40
#define O_ACCMODE (O_RDONLY | O_WRONLY | O_RDWR)
#define O_READ O_RDONLY
#define O_WRITE O_WRONLY
#endif
