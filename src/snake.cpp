#include "snake.h"
#include <string.h>
#include "input.h"

namespace {
  constexpr float kStartStep = 0.15f;  // seconds per move
  constexpr float kMinStep = 0.06f;
}

void SnakeGame::init() { reset(); }

void SnakeGame::reset() {
  memset(occ_, 0, sizeof(occ_));
  len_ = 3;
  head_ = len_ - 1;
  for (int i = 0; i < len_; i++) {
    body_[i] = {int8_t(kCols / 2 - len_ + 1 + i), int8_t(kRows / 2)};
    occ_[body_[i].y * kCols + body_[i].x] = true;
  }
  dx_ = nextDx_ = 1;
  dy_ = nextDy_ = 0;
  score_ = 0;
  timer_ = 0;
  dead_ = false;
  placeFood();
}

void SnakeGame::placeFood() {
  if (len_ >= kMaxLen) return;
  do {
    food_ = {int8_t(random(kCols)), int8_t(random(kRows))};
  } while (occupied(food_.x, food_.y));
}

void SnakeGame::update(float dt) {
  if (dead_) {
    if (Input::justPressed(BTN_START | BTN_A)) reset();
    return;
  }
  // Turning back into yourself is ignored.
  if (Input::pressed(BTN_UP) && dy_ == 0) { nextDx_ = 0; nextDy_ = -1; }
  else if (Input::pressed(BTN_DOWN) && dy_ == 0) { nextDx_ = 0; nextDy_ = 1; }
  else if (Input::pressed(BTN_LEFT) && dx_ == 0) { nextDx_ = -1; nextDy_ = 0; }
  else if (Input::pressed(BTN_RIGHT) && dx_ == 0) { nextDx_ = 1; nextDy_ = 0; }

  float stepTime = kStartStep - score_ * 0.002f;
  if (stepTime < kMinStep) stepTime = kMinStep;
  timer_ += dt;
  while (timer_ >= stepTime && !dead_) {
    timer_ -= stepTime;
    step();
  }
}

void SnakeGame::step() {
  dx_ = nextDx_;
  dy_ = nextDy_;
  Pos h = body_[head_];
  int nx = h.x + dx_, ny = h.y + dy_;
  if (nx < 0 || ny < 0 || nx >= kCols || ny >= kRows) {
    dead_ = true;
    return;
  }

  bool eating = (nx == food_.x && ny == food_.y);
  int tailIdx = (head_ - len_ + 1 + kMaxLen) % kMaxLen;
  if (!eating) occ_[body_[tailIdx].y * kCols + body_[tailIdx].x] = false;  // tail moves away
  if (occupied(nx, ny)) {
    dead_ = true;
    return;
  }

  head_ = (head_ + 1) % kMaxLen;
  body_[head_] = {int8_t(nx), int8_t(ny)};
  occ_[ny * kCols + nx] = true;
  if (eating) {
    len_++;
    score_++;
    placeFood();
  }
}

void SnakeGame::draw(U8G2& g) {
  for (int i = 0; i < len_; i++) {
    const Pos& p = body_[(head_ - i + kMaxLen) % kMaxLen];
    g.drawBox(p.x * kCell, p.y * kCell, kCell - 1, kCell - 1);
  }
  g.drawFrame(food_.x * kCell, food_.y * kCell, kCell - 1, kCell - 1);

  if (dead_) drawMessage(g, "GAME OVER", "Start: again");
}

HudInfo SnakeGame::hud() const {
  HudInfo h;
  h.score = score_;
  return h;
}
