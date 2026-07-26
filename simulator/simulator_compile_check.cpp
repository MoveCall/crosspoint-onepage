// Compile-only canary: pulls in the HAL public headers through the shim layer to
// verify that arduino-host + simulator/shims/ are sufficient for the headers to parse.
// Replaced by simulator_main.cpp once the HAL native implementations exist.

#include <HalDisplay.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <HalSystem.h>
#include <HalTiltSensor.h>

extern "C" void simulator_compile_check_anchor() {
  // Reference one constant from each HAL header so the includes are not "unused".
  (void)HalDisplay::DISPLAY_WIDTH;
  (void)HalDisplay::DISPLAY_HEIGHT;
  (void)HalGPIO::BTN_POWER;
  (void)HalPowerManager::LOW_POWER_FREQ;
}
