#pragma once
#include <Arduino.h>

// Owns Wire and the TCA9548A. Only call from one task (the game task).
namespace I2CBus {
  void begin();
  // Selects a mux channel; no bus traffic if it's already selected.
  bool select(uint8_t channel);
  // Disconnects all downstream channels.
  bool deselectAll();
  // Prints every responding address on each mux channel to Serial.
  void scan();
  // Human-readable text for a Wire.endTransmission() result code.
  const char* errStr(uint8_t code);
}
