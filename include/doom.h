#pragma once
#include "game.h"

// First-person raycaster in the style of doom-nano: grid map, DDA walls in
// fixed point, billboard sprites against a per-column depth buffer, 1-bit
// ordered dithering for distance shading. All code and art are original.
class DoomGame : public Game {
 public:
  const char* name() const override { return "Doom"; }
  void init() override;
  void update(float dt) override;
  void draw(U8G2& g) override;
  HudInfo hud() const override;

  static constexpr int kMapSize = 16;
  static constexpr int kMaxThings = 32;

 private:
  enum class Kind : uint8_t { Enemy, Medkit, Ammo, Exit };
  enum class State : uint8_t { Playing, LevelClear, Dead, Won };

  struct Thing {
    Kind kind;
    bool alive;     // enemies: not dead; pickups: not taken
    bool awake;
    int8_t hp;
    float x, y;
    float attackTimer;
    float hurtTimer;
    float animTimer;
  };

  void loadLevel(int index);
  void updatePlayer(float dt);
  void updateEnemies(float dt);
  void shoot();
  bool isWall(float x, float y) const;
  bool lineOfSight(float x0, float y0, float x1, float y1) const;
  void tryMove(float& x, float& y, float dx, float dy, float radius);

  void renderWalls(uint8_t* buf);
  void renderThings(uint8_t* buf);
  void renderGun(uint8_t* buf);

  char map_[kMapSize][kMapSize];
  Thing things_[kMaxThings];
  int thingCount_ = 0;
  float zbuf_[128];

  float px_ = 0, py_ = 0, angle_ = 0;
  float walkPhase_ = 0;
  int health_ = 100, ammo_ = 20, score_ = 0;
  int level_ = 0, levelStartScore_ = 0;
  float fireCooldown_ = 0, flashTimer_ = 0, hurtFlash_ = 0, stateTimer_ = 0;
  State state_ = State::Playing;
};
