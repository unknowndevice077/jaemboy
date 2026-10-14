#pragma once
#include "game.h"

// Small arcade games. Each is self-contained in its own .cpp.

// Falling blocks: Left/Right move, Down soft drop, Up hard drop, A/B rotate.
class BlocksGame : public Game {
 public:
  const char* name() const override { return "Blocks"; }
  void init() override;
  void update(float dt) override;
  void draw(U8G2& g) override;
  HudInfo hud() const override;

 private:
  static constexpr int kW = 10, kH = 20;
  bool fits(int piece, int rot, int x, int y) const;
  void spawn();
  void lock();
  void tryRotate(int dir);
  void tryShift(int dx);

  uint8_t board_[kH][kW];
  int piece_, rot_, x_, y_, next_;
  float fallTimer_, repeatTimer_;
  int score_, lines_, level_;
  bool dead_;
};

// Breakout-style: Left/Right move the paddle, A launches the ball.
class BricksGame : public Game {
 public:
  const char* name() const override { return "Bricks"; }
  void init() override;
  void update(float dt) override;
  void draw(U8G2& g) override;
  HudInfo hud() const override;

 private:
  static constexpr int kCols = 8, kRows = 6;
  void loadLevel(int level);
  bool hitBrick(float x, float y);

  bool bricks_[kRows][kCols];
  int bricksLeft_;
  float paddleX_, ballX_, ballY_, vx_, vy_;
  bool stuck_;  // ball resting on the paddle, waiting for A
  int score_, lives_, level_;
  bool dead_;
};

// Flappy-style: A (or Up) flaps.
class FlapGame : public Game {
 public:
  const char* name() const override { return "Flap"; }
  void init() override;
  void update(float dt) override;
  void draw(U8G2& g) override;
  HudInfo hud() const override;

 private:
  static constexpr int kPipes = 3;
  float y_, vy_;
  float pipeX_[kPipes];
  int gapY_[kPipes];
  bool passed_[kPipes];
  int score_, best_;
  bool started_, dead_;
};

// Side-scrolling platformer: Left/Right run, A jump (hold for higher),
// hold B to run faster. Stomp enemies, grab coins, bump ? blocks, reach the
// flag.
class JumperGame : public Game {
 public:
  const char* name() const override { return "Jumper"; }
  void init() override;
  void update(float dt) override;
  void draw(U8G2& g) override;
  HudInfo hud() const override;

  static constexpr int kTile = 8;
  static constexpr int kRowsInLevel = 8;
  static constexpr int kMaxCols = 128;
  static constexpr int kMaxEnemies = 16;

 private:
  struct Enemy { float x, y, vx, vy; bool alive; float squash; };

  void loadLevel(int index);
  void respawn();
  char tile(int col, int row) const;
  bool solid(int col, int row) const;
  void moveX(float& x, float y, float& vx, float dx, int w, int h, bool& blocked);
  void moveY(float x, float& y, float& vy, float dy, int w, int h, bool& onGround, int& bumpCol,
             int& bumpRow);
  void die();

  char map_[kRowsInLevel][kMaxCols + 1];
  int cols_;
  Enemy enemies_[kMaxEnemies];
  int enemyCount_;
  float x_, y_, vx_, vy_, camX_;
  bool onGround_, facingLeft_;
  float animTimer_, stateTimer_;
  int score_, coins_, lives_, level_;
  enum class State : uint8_t { Playing, Dying, Clear, GameOver, Won } state_;
};
