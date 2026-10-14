#include <math.h>
#include <string.h>
#include "arcade.h"
#include "input.h"
#include "jumper_levels.h"

namespace {

constexpr int T = JumperGame::kTile;
constexpr int kPlayerW = 6, kPlayerH = 8;
constexpr int kEnemyW = 7, kEnemyH = 7;
constexpr float kWalk = 60, kRun = 95;
constexpr float kAccelGround = 420, kAccelAir = 260, kFriction = 380;
constexpr float kGravity = 540, kMaxFall = 220;
constexpr float kJump = -195, kJumpCut = -60;
constexpr float kEnemySpeed = 20;

// 8x8 art: '#' white, '.' black, ' ' transparent. Original designs.
const char* const kHeroStand[8] = {
  "  ####  ", " ###### ", " #.##.# ", " ###### ",
  "  #..#  ", " ###### ", "  #  #  ", " ##  ## ",
};
const char* const kHeroRun[8] = {
  "  ####  ", " ###### ", " #.##.# ", " ###### ",
  "  #..#  ", " ###### ", " #    # ", "##    ##",
};
const char* const kHeroJump[8] = {
  "  ####  ", " ###### ", " #.##.# ", " ###### ",
  "# #..# #", "########", "  #  #  ", " #    # ",
};
const char* const kWalkerA[8] = {
  "  ####  ", " ###### ", "##.##.##", "##.##.##",
  "########", " #....# ", "  #  #  ", " ##  ## ",
};
const char* const kWalkerB[8] = {
  "  ####  ", " ###### ", "##.##.##", "##.##.##",
  "########", " #....# ", " #    # ", "##    ##",
};
const char* const kWalkerFlat[8] = {
  "        ", "        ", "        ", "        ",
  "        ", "  ####  ", " #.##.# ", "########",
};

// Direct buffer drawing with signed, clipped coordinates (U8g2's 8-bit
// coordinates wrap for things partly off the left edge).
uint8_t* fb;

inline void px(int x, int y, bool on) {
  if (x < 0 || x >= 128 || y < 0 || y >= 64) return;
  uint8_t& b = fb[(y >> 3) * 128 + x];
  if (on) b |= 1 << (y & 7);
  else b &= ~(1 << (y & 7));
}
void rect(int x, int y, int w, int h, bool on = true) {
  for (int j = y; j < y + h; j++)
    for (int i = x; i < x + w; i++) px(i, j, on);
}
void frame(int x, int y, int w, int h) {
  rect(x, y, w, 1);
  rect(x, y + h - 1, w, 1);
  rect(x, y, 1, h);
  rect(x + w - 1, y, 1, h);
}
void art(const char* const rows[8], int x, int y, bool flip = false) {
  for (int r = 0; r < 8; r++)
    for (int c = 0; c < 8; c++) {
      char ch = rows[r][flip ? 7 - c : c];
      if (ch != ' ') px(x + c, y + r, ch == '#');
    }
}

bool isSolid(char t) { return t == '#' || t == 'B' || t == '?' || t == 'U' || t == 'p'; }

}  // namespace

// ---------------------------------------------------------------- setup

void JumperGame::init() {
  score_ = coins_ = 0;
  lives_ = 3;
  loadLevel(0);
}

void JumperGame::loadLevel(int index) {
  level_ = index;
  enemyCount_ = 0;
  cols_ = strlen(kJumperLevels[index][0]);
  if (cols_ > kMaxCols) cols_ = kMaxCols;
  for (int r = 0; r < kRowsInLevel; r++) {
    for (int c = 0; c < cols_; c++) {
      char ch = kJumperLevels[index][r][c];
      map_[r][c] = ch;
      if (ch == 'P') {
        map_[r][c] = '.';
      } else if (ch == 'e') {
        map_[r][c] = '.';
        if (enemyCount_ < kMaxEnemies)
          enemies_[enemyCount_++] = {float(c * T), float(r * T + T - kEnemyH), -kEnemySpeed, 0,
                                     true, 0};
      }
    }
    map_[r][cols_] = 0;
  }
  respawn();
}

void JumperGame::respawn() {
  for (int r = 0; r < kRowsInLevel; r++)
    for (int c = 0; c < cols_; c++)
      if (kJumperLevels[level_][r][c] == 'P') {
        x_ = c * T + 1;
        y_ = r * T;
      }
  vx_ = vy_ = 0;
  camX_ = 0;
  onGround_ = facingLeft_ = false;
  animTimer_ = stateTimer_ = 0;
  state_ = State::Playing;
}

char JumperGame::tile(int col, int row) const {
  if (row < 0 || row >= kRowsInLevel) return '.';
  if (col < 0 || col >= cols_) return '#';  // invisible side walls
  return map_[row][col];
}

bool JumperGame::solid(int col, int row) const { return isSolid(tile(col, row)); }

// ---------------------------------------------------------------- physics

void JumperGame::moveX(float& x, float y, float& vx, float dx, int w, int h, bool& blocked) {
  blocked = false;
  x += dx;
  int top = (int)floorf(y) / T, bottom = (int)floorf(y + h - 1) / T;
  if (dx > 0) {
    int col = (int)floorf(x + w - 1) / T;
    for (int r = top; r <= bottom; r++)
      if (solid(col, r)) { x = col * T - w; vx = 0; blocked = true; return; }
  } else if (dx < 0) {
    int col = (int)floorf(x) / T;
    if (x < 0) col = -1;
    for (int r = top; r <= bottom; r++)
      if (solid(col, r)) { x = (col + 1) * T; vx = 0; blocked = true; return; }
  }
}

void JumperGame::moveY(float x, float& y, float& vy, float dy, int w, int h, bool& onGround,
                       int& bumpCol, int& bumpRow) {
  onGround = false;
  bumpCol = -1;
  y += dy;
  int left = (int)floorf(x) / T, right = (int)floorf(x + w - 1) / T;
  if (dy > 0) {
    int row = (int)floorf(y + h - 1) / T;
    if (y + h - 1 < 0) return;
    for (int c = left; c <= right; c++)
      if (solid(c, row)) { y = row * T - h; vy = 0; onGround = true; return; }
  } else if (dy < 0) {
    if (y < 0) return;
    int row = (int)floorf(y) / T;
    // Bump the block under the player's centre if there is one.
    int mid = (int)floorf(x + w / 2) / T;
    for (int c : {mid, left, right})
      if (solid(c, row)) {
        y = (row + 1) * T;
        vy = 0;
        bumpCol = c;
        bumpRow = row;
        return;
      }
  }
}

void JumperGame::die() {
  state_ = State::Dying;
  stateTimer_ = 1.4f;
  vy_ = -160;
}

void JumperGame::update(float dt) {
  if (dt > 0.05f) dt = 0.05f;
  animTimer_ += dt;

  switch (state_) {
    case State::Dying:
      // Pop up and fall off the screen, then restart the level.
      vy_ += kGravity * dt;
      y_ += vy_ * dt;
      stateTimer_ -= dt;
      if (stateTimer_ <= 0) {
        if (--lives_ <= 0) state_ = State::GameOver;
        else loadLevel(level_);
      }
      return;
    case State::Clear:
      stateTimer_ -= dt;
      if (stateTimer_ <= 0) loadLevel(level_ + 1);
      return;
    case State::GameOver:
    case State::Won:
      if (Input::justPressed(BTN_START | BTN_A)) init();
      return;
    case State::Playing:
      break;
  }

  // Run / walk toward the target speed.
  int dir = Input::pressed(BTN_LEFT) ? -1 : Input::pressed(BTN_RIGHT) ? 1 : 0;
  float target = dir * (Input::pressed(BTN_B) ? kRun : kWalk);
  float accel = dir ? (onGround_ ? kAccelGround : kAccelAir) : (onGround_ ? kFriction : 60);
  if (vx_ < target) vx_ = fminf(vx_ + accel * dt, target);
  else if (vx_ > target) vx_ = fmaxf(vx_ - accel * dt, target);
  if (dir) facingLeft_ = dir < 0;

  // Jump; letting go of A early cuts the jump short.
  if (Input::justPressed(BTN_A) && onGround_) vy_ = kJump;
  if (!Input::pressed(BTN_A) && vy_ < kJumpCut) vy_ = kJumpCut;
  vy_ = fminf(vy_ + kGravity * dt, kMaxFall);

  bool blocked;
  moveX(x_, y_, vx_, vx_ * dt, kPlayerW, kPlayerH, blocked);
  if (x_ < camX_) { x_ = camX_; vx_ = 0; }  // the screen only scrolls forward
  int bumpCol, bumpRow;
  moveY(x_, y_, vy_, vy_ * dt, kPlayerW, kPlayerH, onGround_, bumpCol, bumpRow);
  if (bumpCol >= 0 && map_[bumpRow][bumpCol] == '?') {
    map_[bumpRow][bumpCol] = 'U';
    coins_++;
    score_ += 50;
  }

  // Coins.
  for (int r = (int)y_ / T; r <= (int)(y_ + kPlayerH - 1) / T; r++)
    for (int c = (int)x_ / T; c <= (int)(x_ + kPlayerW - 1) / T; c++)
      if (r >= 0 && r < kRowsInLevel && c >= 0 && c < cols_ && map_[r][c] == 'o') {
        map_[r][c] = '.';
        coins_++;
        score_ += 50;
      }

  if (y_ > 64) {  // fell into a pit
    die();
    return;
  }

  // Flag.
  int flagCol = (int)(x_ + kPlayerW) / T;
  for (int r = 0; r < kRowsInLevel; r++)
    if (flagCol < cols_ && map_[r][flagCol] == 'F') {
      score_ += 500;
      if (level_ + 1 >= kJumperLevelCount) {
        state_ = State::Won;
      } else {
        state_ = State::Clear;
        stateTimer_ = 2.0f;
      }
      return;
    }

  // Enemies wake up just before they scroll into view.
  for (int i = 0; i < enemyCount_; i++) {
    Enemy& e = enemies_[i];
    if (!e.alive) {
      e.squash -= dt;
      continue;
    }
    if (e.x > camX_ + 128 + 16) continue;
    e.vy = fminf(e.vy + kGravity * dt, kMaxFall);
    bool eb, eg;
    float prevVx = e.vx;
    moveX(e.x, e.y, e.vx, e.vx * dt, kEnemyW, kEnemyH, eb);
    if (eb) e.vx = -prevVx;  // turn around at walls
    int bc, br;
    moveY(e.x, e.y, e.vy, e.vy * dt, kEnemyW, kEnemyH, eg, bc, br);
    if (e.y > 64) {
      e.alive = false;
      continue;
    }

    bool overlap = x_ < e.x + kEnemyW && x_ + kPlayerW > e.x && y_ < e.y + kEnemyH &&
                   y_ + kPlayerH > e.y;
    if (!overlap) continue;
    if (vy_ > 0 && y_ + kPlayerH - e.y < 6) {
      e.alive = false;
      e.squash = 0.4f;
      vy_ = Input::pressed(BTN_A) ? kJump * 0.8f : -120;
      score_ += 100;
    } else {
      die();
      return;
    }
  }

  float targetCam = x_ - 48;
  if (targetCam > camX_) camX_ = targetCam;
  if (camX_ > cols_ * T - 128) camX_ = cols_ * T - 128;
}

// ---------------------------------------------------------------- drawing

void JumperGame::draw(U8G2& g) {
  fb = g.getBufferPtr();
  int cam = (int)camX_;
  int c0 = cam / T;

  for (int r = 0; r < kRowsInLevel; r++) {
    for (int c = c0; c <= c0 + 128 / T && c < cols_; c++) {
      int x = c * T - cam, y = r * T;
      switch (map_[r][c]) {
        case '#':
          if (!solid(c, r - 1)) {  // grass edge on top of ground
            rect(x, y, T, T);
            for (int i = 0; i < T; i += 2) px(x + i, y + 2, false);
          } else {
            rect(x, y, T, T);
            px(x + 2, y + 3, false);
            px(x + 6, y + 6, false);
          }
          break;
        case 'B':
          frame(x, y, T, T);
          rect(x, y + 3, T, 1);
          rect(x + 3, y, 1, 3);
          rect(x + 6, y + 4, 1, 4);
          break;
        case '?':
          frame(x, y, T, T);
          rect(x + 3, y + 2, 2, 1);
          px(x + 5, y + 3, true);
          px(x + 4, y + 4, true);
          px(x + 4, y + 6, true);
          break;
        case 'U':
          rect(x, y, T, T);
          px(x + 1, y + 1, false); px(x + 6, y + 1, false);
          px(x + 1, y + 6, false); px(x + 6, y + 6, false);
          break;
        case 'p': {
          bool right = tile(c - 1, r) == 'p' && tile(c - 2, r) != 'p';
          bool top = tile(c, r - 1) != 'p';
          if (top) frame(right ? x - 1 : x, y, T + 1, 4);
          rect(right ? x + 6 : x + 1, top ? y + 4 : y, 1, top ? 4 : T);
          rect(right ? x + 3 : x + 3, top ? y + 4 : y, 1, top ? 4 : T);
          break;
        }
        case 'o': {
          // Spinning coin: width pulses.
          int w = ((int)(animTimer_ * 6) & 1) ? 2 : 4;
          rect(x + 4 - w / 2, y + 2, w, 5);
          break;
        }
        case 'F':
          rect(x + 3, y, 1, T);
          if (tile(c, r - 1) != 'F') {
            rect(x + 2, y - 2, 3, 2);
            for (int i = 0; i < 6; i++) rect(x + 3 - 6 + i, y + 1 + i / 2, 6 - i, 1);
          }
          break;
      }
    }
  }

  for (int i = 0; i < enemyCount_; i++) {
    const Enemy& e = enemies_[i];
    int ex = (int)e.x - cam - 1, ey = (int)e.y + kEnemyH - 8;
    if (e.alive) art(((int)(animTimer_ * 4) & 1) ? kWalkerB : kWalkerA, ex, ey);
    else if (e.squash > 0) art(kWalkerFlat, ex, ey);
  }

  const char* const* hero = kHeroStand;
  if (state_ == State::Dying || !onGround_) hero = kHeroJump;
  else if (fabsf(vx_) > 5 && ((int)(animTimer_ * 10) & 1)) hero = kHeroRun;
  art(hero, (int)x_ - cam - 1, (int)y_, facingLeft_);

  char line[20];
  switch (state_) {
    case State::Clear:
      snprintf(line, sizeof(line), "LEVEL %d CLEAR", level_ + 1);
      drawMessage(g, line, nullptr);
      break;
    case State::GameOver:
      drawMessage(g, "GAME OVER", "Start: again");
      break;
    case State::Won:
      drawMessage(g, "YOU WIN!", "Start: again");
      break;
    default:
      break;
  }
}

HudInfo JumperGame::hud() const {
  HudInfo h;
  h.lives = lives_;
  h.score = score_;
  h.level = level_ + 1;
  return h;
}
