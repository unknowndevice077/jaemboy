#pragma once
#include <Arduino.h>

// WiFi access point + controller page + WebSocket. The phone sends one byte
// (the button mask, see input.h) whenever its buttons change.
namespace Net {
  void begin();
  bool apUp();
  uint8_t clients();
  const char* ip();
}
