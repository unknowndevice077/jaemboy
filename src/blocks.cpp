#include <string.h>
#include "arcade.h"
#include "input.h"

namespace {

// 7 pieces x 4 rotations, each a 4x4 grid packed into 16 bits (row-major,
// top-left = bit 15).
const uint16_t kPieces[7][4] = {
  {0x0F00, 0x2222, 0x00F0, 0x4444},  // I
  {0x8E00, 0x6440, 0x0E20, 0x44C0},  // J
  {0x2E00, 0x4460, 0x0E80, 0xC440},  // L
  {0x6600, 0x6600, 0x6600, 0x6600},  // O
  {0x6C00, 0x4620, 0x06C0, 0x8C40},  // S
  {0x4E00, 0x4640, 0x0E40, 0x4C40},  // T
  {0xC600, 0x2640, 0x0C60, 0x4C80},  // Z
};
inline bool cell(int piece, int rot, int r, int c) {
  return kPieces[piece][rot & 3] >> (15 - (r * 4 + c)) & 1;
}

constexpr int kCell = 3;
constexpr int kBoardX = (128 - 10 * kCell) / 2;
constexpr int kBoardY = 2;
constexpr float kRepeatDelay = 0.17f, kRepeatRate = 0.05f;

}  // namespace

void BlocksGame::init() {
  memset(board_, 0, sizeof(board_));
  score_ = lines_ = 0;
  level_ = 1;
  dead_ = false;
  next_ = random(7);
  spawn();
}

bool BlocksGame::fits(int piece, int rot, int x, int y) const {
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 4; c++) {
      if (!cell(piece, rot, r, c)) continue;
      int bx = x + c, by = y + r;
      if (bx < 0 || bx >= kW || by >= kH) return false;
      if (by >= 0 && board_[by][bx]) return false;
    }
  return true;
}

void BlocksGame::spawn() {
  piece_ = next_;
  next_ = random(7);
  rot_ = 0;
  x_ = 3;
  y_ = 0;
  fallTimer_ = 0;
  if (!fits(piece_, rot_, x_, y_)) dead_ = true;
}

void BlocksGame::lock() {
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 4; c++)
      if (cell(piece_, rot_, r, c) && y_ + r >= 0) board_[y_ + r][x_ + c] = 1;

  int cleared = 0;
  for (int r = kH - 1; r >= 0; r--) {
    bool full = true;
    for (int c = 0; c < kW && full; c++) full = board_[r][c];
    if (!full) continue;
    memmove(board_[1], board_[0], r * kW);
    memset(board_[0], 0, kW);
    cleared++;
    r++;  // re-check the row that just moved down into this one
  }
  static const int kLineScore[5] = {0, 40, 100, 300, 1200};
  score_ += kLineScore[cleared] * level_;
  lines_ += cleared;
  level_ = lines_ / 10 + 1;
  spawn();
}

void BlocksGame::tryRotate(int dir) {
  int r = (rot_ + dir) & 3;
  static const int kKicks[] = {0, -1, 1, -2, 2};
  for (int k : kKicks)
    if (fits(piece_, r, x_ + k, y_)) {
      rot_ = r;
      x_ += k;
      return;
    }
}

void BlocksGame::tryShift(int dx) {
  if (fits(piece_, rot_, x_ + dx, y_)) x_ += dx;
}

void BlocksGame::update(float dt) {
  if (dead_) {
    if (Input::justPressed(BTN_START | BTN_A)) init();
    return;
  }
  if (Input::justPressed(BTN_A)) tryRotate(1);
  if (Input::justPressed(BTN_B)) tryRotate(-1);

  // Left/Right: one step on press, then auto-repeat while held.
  int dir = Input::pressed(BTN_LEFT) ? -1 : Input::pressed(BTN_RIGHT) ? 1 : 0;
  if (Input::justPressed(BTN_LEFT | BTN_RIGHT)) {
    tryShift(dir);
    repeatTimer_ = kRepeatDelay;
  } else if (dir) {
    repeatTimer_ -= dt;
    if (repeatTimer_ <= 0) {
      tryShift(dir);
      repeatTimer_ = kRepeatRate;
    }
  }

  if (Input::justPressed(BTN_UP)) {
    while (fits(piece_, rot_, x_, y_ + 1)) {
      y_++;
      score_ += 2;
    }
    lock();
    return;
  }

  float interval = 0.8f - (level_ - 1) * 0.07f;
  if (interval < 0.08f) interval = 0.08f;
  if (Input::pressed(BTN_DOWN)) interval = 0.04f;
  fallTimer_ += dt;
  if (fallTimer_ >= interval) {
    fallTimer_ = 0;
    if (fits(piece_, rot_, x_, y_ + 1)) y_++;
    else lock();
  }
}

void BlocksGame::draw(U8G2& g) {
  g.drawFrame(kBoardX - 2, kBoardY - 2, kW * kCell + 4, kH * kCell + 4);
  for (int r = 0; r < kH; r++)
    for (int c = 0; c < kW; c++)
      if (board_[r][c]) g.drawBox(kBoardX + c * kCell, kBoardY + r * kCell, kCell, kCell);

  if (!dead_) {
    // Ghost: outline where the piece would land.
    int gy = y_;
    while (fits(piece_, rot_, x_, gy + 1)) gy++;
    for (int r = 0; r < 4; r++)
      for (int c = 0; c < 4; c++) {
        if (!cell(piece_, rot_, r, c)) continue;
        int px = kBoardX + (x_ + c) * kCell;
        if (gy + r >= 0) g.drawPixel(px + 1, kBoardY + (gy + r) * kCell + 1);
        if (y_ + r >= 0) g.drawBox(px, kBoardY + (y_ + r) * kCell, kCell, kCell);
      }
  }

  // Next piece, to the right of the board.
  g.setFont(u8g2_font_5x7_tf);
  g.drawStr(92, 10, "NEXT");
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 4; c++)
      if (cell(next_, 0, r, c)) g.drawBox(94 + c * 4, 14 + r * 4, 3, 3);

  if (dead_) drawMessage(g, "GAME OVER", "Start: again");
}

HudInfo BlocksGame::hud() const {
  HudInfo h;
  h.score = score_;
  h.level = level_;
  return h;
}
