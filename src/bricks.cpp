#include <math.h>
#include "arcade.h"
#include "input.h"

namespace {
constexpr int kBrickW = 16, kBrickH = 4, kTop = 4;
constexpr int kPaddleW = 20, kPaddleY = 60;
constexpr float kPaddleSpeed = 120;
constexpr int kBall = 2;
}  // namespace

void BricksGame::init() {
  score_ = 0;
  lives_ = 3;
  dead_ = false;
  loadLevel(1);
}

void BricksGame::loadLevel(int level) {
  level_ = level;
  bricksLeft_ = 0;
  // Later levels add rows and punch a pattern of gaps into the wall.
  int rows = level + 2 > kRows ? kRows : level + 2;
  for (int r = 0; r < kRows; r++)
    for (int c = 0; c < kCols; c++) {
      bool on = r < rows && !(level >= 3 && (r + c) % 4 == 0);
      bricks_[r][c] = on;
      bricksLeft_ += on;
    }
  paddleX_ = (128 - kPaddleW) / 2;
  stuck_ = true;
}

bool BricksGame::hitBrick(float x, float y) {
  int c = (int)x / kBrickW, r = ((int)y - kTop) / (kBrickH + 1);
  if (y < kTop || c < 0 || c >= kCols || r < 0 || r >= kRows || !bricks_[r][c]) return false;
  bricks_[r][c] = false;
  bricksLeft_--;
  score_ += 10 * level_;
  return true;
}

void BricksGame::update(float dt) {
  if (dead_) {
    if (Input::justPressed(BTN_START | BTN_A)) init();
    return;
  }
  if (Input::pressed(BTN_LEFT)) paddleX_ -= kPaddleSpeed * dt;
  if (Input::pressed(BTN_RIGHT)) paddleX_ += kPaddleSpeed * dt;
  if (paddleX_ < 0) paddleX_ = 0;
  if (paddleX_ > 128 - kPaddleW) paddleX_ = 128 - kPaddleW;

  if (stuck_) {
    ballX_ = paddleX_ + kPaddleW / 2 - 1;
    ballY_ = kPaddleY - kBall - 1;
    if (Input::justPressed(BTN_A | BTN_UP | BTN_START)) {
      stuck_ = false;
      float speed = 70 + level_ * 8;
      vx_ = speed * 0.6f * (random(2) ? 1 : -1);
      vy_ = -speed * 0.8f;
    }
    return;
  }

  // Move in small sub-steps so the ball can't skip through a brick.
  int steps = (int)(fmaxf(fabsf(vx_), fabsf(vy_)) * dt) + 1;
  float sdt = dt / steps;
  for (int i = 0; i < steps; i++) {
    float nx = ballX_ + vx_ * sdt;
    if (nx < 0 || nx > 128 - kBall) vx_ = -vx_;
    else if (hitBrick(nx + (vx_ > 0 ? kBall : 0), ballY_ + 1)) vx_ = -vx_;
    else ballX_ = nx;

    float ny = ballY_ + vy_ * sdt;
    if (ny < 0) vy_ = -vy_;
    else if (hitBrick(ballX_ + 1, ny + (vy_ > 0 ? kBall : 0))) vy_ = -vy_;
    else if (vy_ > 0 && ny + kBall >= kPaddleY && ballY_ + kBall <= kPaddleY &&
             ballX_ + kBall >= paddleX_ && ballX_ <= paddleX_ + kPaddleW) {
      // Bounce angle depends on where the ball hits the paddle.
      float hit = (ballX_ + 1 - (paddleX_ + kPaddleW / 2)) / (kPaddleW / 2);
      float speed = sqrtf(vx_ * vx_ + vy_ * vy_) * 1.01f;
      vx_ = speed * hit * 0.8f;
      vy_ = -sqrtf(fmaxf(speed * speed - vx_ * vx_, speed * speed * 0.2f));
    } else ballY_ = ny;
  }

  if (ballY_ > 64) {
    if (--lives_ <= 0) dead_ = true;
    stuck_ = true;
  }
  if (bricksLeft_ == 0) loadLevel(level_ + 1);
}

void BricksGame::draw(U8G2& g) {
  for (int r = 0; r < kRows; r++)
    for (int c = 0; c < kCols; c++)
      if (bricks_[r][c]) g.drawBox(c * kBrickW + 1, kTop + r * (kBrickH + 1), kBrickW - 2, kBrickH);
  g.drawBox((int)paddleX_, kPaddleY, kPaddleW, 2);
  g.drawBox((int)ballX_, (int)ballY_, kBall, kBall);
  if (dead_) drawMessage(g, "GAME OVER", "Start: again");
  else if (stuck_) {
    g.setFont(u8g2_font_5x7_tf);
    g.drawStr(44, 48, "A: launch");
  }
}

HudInfo BricksGame::hud() const {
  HudInfo h;
  h.lives = lives_;
  h.score = score_;
  h.level = level_;
  return h;
}
