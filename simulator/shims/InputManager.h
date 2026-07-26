#pragma once

// Shim: lib/hal/HalGPIO.h includes this unconditionally. Under CROSSPOINT_EMULATED=1
// the actual InputManager member is excluded (see HalGPIO.h:42), so only the type
// name needs to exist for parsing.

class InputManager {
 public:
  InputManager() = default;
  static constexpr int POWER_BUTTON_PIN = 3;
};
