#pragma once

// Build-time knobs for the CrossPoint Reader simulator. Override via -D... or by
// editing this file. Runtime overrides come via CLI flags parsed in simulator_main.cpp.

#ifndef SIMULATOR_WINDOW_SCALE
#define SIMULATOR_WINDOW_SCALE 1
#endif

#ifndef SIMULATOR_BATTERY_PERCENT
#define SIMULATOR_BATTERY_PERCENT 87
#endif

// If non-zero, sleep N ms inside HalDisplay::displayBuffer to mimic e-ink refresh time.
#ifndef SIMULATE_REFRESH_DELAY_MS
#define SIMULATE_REFRESH_DELAY_MS 0
#endif

// Default SD card root (filesystem prefix). Override at runtime via --sd-root.
#ifndef SIMULATOR_DEFAULT_SD_ROOT
#define SIMULATOR_DEFAULT_SD_ROOT "./simulator/sd_root"
#endif
