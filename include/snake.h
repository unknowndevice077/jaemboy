#pragma once
#include "game.h"

class SnakeGame : public Game {
 public:
  const char* name() const override { return "Snake"; }
  void init() override;
  void update(float dt) override;
  void draw(U8G2& g) override;
  HudInfo hud() const override;

  static constexpr int kCell = 4;
  static constexpr int kCols = 128 / kCell;
  static constexpr int kRows = 64 / kCell;
  static constexpr int kMaxLen = kCols * kRows;

 private:
  struct Pos { int8_t x, y; };

  void reset();
  void step();
  void placeFood();
  bool occupied(int x, int y) const { return occ_[y * kCols + x]; }

  Pos body_[kMaxLen];  // ring buffer, head at head_
  bool occ_[kMaxLen];
  int head_ = 0, len_ = 0;
  int8_t dx_ = 1, dy_ = 0;      // current direction
  int8_t nextDx_ = 1, nextDy_ = 0;
  Pos food_;
  float timer_ = 0;
  int score_ = 0;
  bool dead_ = false;
};
