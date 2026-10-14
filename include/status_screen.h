#pragma once
#include "game.h"
#include "oled.h"

// Everything OLED 1 shows. The screen is redrawn and flushed only when this
// changes.
struct StatusModel {
  bool inGame = false;
  uint8_t cursor = 0;   // launcher selection / running game index
  HudInfo hud;
  int16_t fps = -1;     // -1 hides it
  bool apUp = false;
  uint8_t clients = 0;  // connected controllers

  bool operator==(const StatusModel& o) const {
    return inGame == o.inGame && cursor == o.cursor && hud == o.hud && fps == o.fps &&
           apUp == o.apUp && clients == o.clients;
  }
  bool operator!=(const StatusModel& o) const { return !(*this == o); }
};

namespace StatusScreen {
  void begin(Oled& oled);
  // Returns true if the screen was redrawn.
  bool show(const StatusModel& m);
}
