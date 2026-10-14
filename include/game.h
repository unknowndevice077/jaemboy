#pragma once
#include <U8g2lib.h>

// Status a game reports for the HUD on OLED 1. -1 hides a field.
struct HudInfo {
  int16_t health = -1;
  int16_t ammo = -1;
  int16_t score = -1;
  int16_t level = -1;
  int16_t lives = -1;

  bool operator==(const HudInfo& o) const {
    return health == o.health && ammo == o.ammo && score == o.score && level == o.level &&
           lives == o.lives;
  }
  bool operator!=(const HudInfo& o) const { return !(*this == o); }
};

class Game {
 public:
  virtual ~Game() = default;
  virtual const char* name() const = 0;
  virtual void init() = 0;
  virtual void update(float dt) = 0;  // dt in seconds
  virtual void draw(U8G2& g) = 0;     // draw into the game screen's buffer
  virtual void exit() {}
  virtual HudInfo hud() const { return HudInfo(); }
};

// Defined in games.cpp.
extern Game* const kGames[];
extern const uint8_t kGameCount;

// Centred boxed message ("GAME OVER" etc.) over the game screen. line2 may be
// null.
void drawMessage(U8G2& g, const char* line1, const char* line2);
