#pragma once
#include <Arduino.h>

// One bit per button. This is also the WebSocket wire format.
enum Button : uint8_t {
  BTN_UP     = 1 << 0,
  BTN_DOWN   = 1 << 1,
  BTN_LEFT   = 1 << 2,
  BTN_RIGHT  = 1 << 3,
  BTN_A      = 1 << 4,
  BTN_B      = 1 << 5,
  BTN_START  = 1 << 6,
  BTN_SELECT = 1 << 7,
};

// Input sources write their own mask from any task; games read a per-frame
// snapshot. Presses and releases are latched between frames, so a tap shorter
// than one frame still shows up in justPressed().
namespace Input {
  enum Source : uint8_t { SRC_SERIAL, SRC_WEB, SRC_COUNT };

  void setSource(Source src, uint8_t mask);

  // Game task, once per frame, before update().
  void update();

  bool pressed(uint8_t buttons);       // any of the buttons is held
  bool justPressed(uint8_t buttons);   // went down since the last frame
  bool justReleased(uint8_t buttons);  // went up since the last frame
  // Horizontal swipe on the phone's look area since the last frame, already
  // scaled by the page's sensitivity setting. Positive = right.
  int look();
  void addLook(int delta);  // from any task

  // Reads serial keys into SRC_SERIAL (no-op unless SERIAL_INPUT). Returns
  // the last key that isn't a button (for debug commands), or 0.
  char pollSerial();
}
