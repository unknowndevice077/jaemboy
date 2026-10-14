#include "arcade.h"
#include "input.h"

namespace {
constexpr float kBirdX = 30;
constexpr int kBirdW = 7, kBirdH = 5;
constexpr float kGravity = 260, kFlap = -88;
constexpr float kPipeSpeed = 42;
constexpr int kPipeW = 10, kGap = 24, kSpacing = 50;
}  // namespace

void FlapGame::init() {
  y_ = 28;
  vy_ = 0;
  for (int i = 0; i < kPipes; i++) {
    pipeX_[i] = 128 + i * kSpacing;
    gapY_[i] = 8 + random(64 - kGap - 16);
    passed_[i] = false;
  }
  score_ = 0;
  started_ = dead_ = false;
}

void FlapGame::update(float dt) {
  bool flap = Input::justPressed(BTN_A | BTN_UP);
  if (dead_) {
    if (Input::justPressed(BTN_START | BTN_A)) init();
    return;
  }
  if (!started_) {
    if (!flap) return;
    started_ = true;
  }
  if (flap) vy_ = kFlap;
  vy_ += kGravity * dt;
  y_ += vy_ * dt;
  if (y_ < 0 || y_ + kBirdH > 64) dead_ = true;

  for (int i = 0; i < kPipes; i++) {
    pipeX_[i] -= kPipeSpeed * dt;
    if (pipeX_[i] < -kPipeW) {
      // Recycle behind the right-most pipe.
      float maxX = 0;
      for (float x : pipeX_) maxX = x > maxX ? x : maxX;
      pipeX_[i] = maxX + kSpacing;
      gapY_[i] = 8 + random(64 - kGap - 16);
      passed_[i] = false;
    }
    bool overlapX = kBirdX + kBirdW > pipeX_[i] && kBirdX < pipeX_[i] + kPipeW;
    if (overlapX && (y_ < gapY_[i] || y_ + kBirdH > gapY_[i] + kGap)) dead_ = true;
    if (!passed_[i] && pipeX_[i] + kPipeW < kBirdX) {
      passed_[i] = true;
      score_++;
    }
  }
  if (dead_ && score_ > best_) best_ = score_;
}

void FlapGame::draw(U8G2& g) {
  for (int i = 0; i < kPipes; i++) {
    int x = (int)pipeX_[i];
    g.drawFrame(x, -1, kPipeW, gapY_[i] + 1);
    g.drawFrame(x, gapY_[i] + kGap, kPipeW, 64 - gapY_[i] - kGap + 1);
    g.drawBox(x - 1, gapY_[i] - 3, kPipeW + 2, 3);  // pipe lips
    g.drawBox(x - 1, gapY_[i] + kGap, kPipeW + 2, 3);
  }
  // Bird: body, eye, and a wing that flips with vertical speed.
  int by = (int)y_;
  g.drawRBox((int)kBirdX, by, kBirdW, kBirdH, 2);
  g.setDrawColor(0);
  g.drawPixel((int)kBirdX + 5, by + 1);
  g.drawHLine((int)kBirdX + 1, by + (vy_ < 0 ? 1 : 3), 3);
  g.setDrawColor(1);

  if (!started_) drawMessage(g, "FLAP", "A: flap");
  else if (dead_) {
    char line[20];
    snprintf(line, sizeof(line), "BEST %d", best_);
    drawMessage(g, "GAME OVER", line);
  }
}

HudInfo FlapGame::hud() const {
  HudInfo h;
  h.score = score_;
  return h;
}
