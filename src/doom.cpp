#include "doom.h"
#include <math.h>
#include <string.h>
#include "config.h"
#include "doom_assets.h"
#include "input.h"

namespace {

constexpr int W = OLED_WIDTH;
constexpr int H = OLED_HEIGHT;

// Fixed point: positions and distances are Q16 (1.0 = 65536), directions Q14.
constexpr int kAngleSteps = 1024;
constexpr int32_t kPlaneQ14 = 10813;  // 0.66: ~66 degree horizontal FOV
// Wall height in pixels at distance 1.0. W / (2 * 0.66) keeps blocks square.
constexpr int32_t kWallScale = 97;

constexpr float kMoveSpeed = 2.6f;   // tiles/s
constexpr float kTurnSpeed = 2.8f;   // rad/s
constexpr float kLookRadPerUnit = 0.005f;  // phone swipe: ~300 px for 90 degrees
constexpr float kPlayerRadius = 0.22f;
constexpr float kEnemySpeed = 1.3f;
constexpr float kEnemyReach = 0.85f;
constexpr float kEnemyWake = 7.0f;
constexpr float kFireDelay = 0.35f;
constexpr int kStartHealth = 100;
constexpr int kStartAmmo = 20;

int16_t sinTab[kAngleSteps];  // Q14
int16_t camTab[W];            // camera x per column, -1..1 in Q14
bool tablesReady = false;

const uint8_t kBayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

// Sprite art compiled from the ASCII in doom_assets.h: shape bit = white,
// mask bit = opaque. Bit 0 is the leftmost pixel.
struct Art16 { uint16_t shape[16], mask[16]; };
struct Art32 { uint32_t shape[16], mask[16]; };
enum ArtId { ART_WALK_A, ART_WALK_B, ART_ATTACK, ART_DEAD, ART_MEDKIT, ART_AMMO, ART_EXIT, ART_COUNT };
Art16 art[ART_COUNT];
Art32 gunArt;

template <typename T>
void compileArt(const char* const rows[16], int width, T* shape, T* mask) {
  for (int y = 0; y < 16; y++) {
    shape[y] = mask[y] = 0;
    for (int x = 0; x < width; x++) {
      char c = rows[y][x];
      if (c != ' ') mask[y] |= T(1) << x;
      if (c == '#') shape[y] |= T(1) << x;
    }
  }
}

void initTables() {
  if (tablesReady) return;
  for (int i = 0; i < kAngleSteps; i++)
    sinTab[i] = (int16_t)lroundf(sinf(i * 2 * PI / kAngleSteps) * 16384);
  for (int x = 0; x < W; x++) camTab[x] = (int16_t)((2 * x - W + 1) * 16384 / W);
  const char* const* src[ART_COUNT] = {kArtEnemyWalkA, kArtEnemyWalkB, kArtEnemyAttack,
                                       kArtEnemyDead, kArtMedkit, kArtAmmo, kArtExit};
  for (int i = 0; i < ART_COUNT; i++) compileArt(src[i], 16, art[i].shape, art[i].mask);
  compileArt(kArtGun, 32, gunArt.shape, gunArt.mask);
  tablesReady = true;
}

inline int angleIndex(float a) {
  return (int)lroundf(a * (kAngleSteps / (2 * PI))) & (kAngleSteps - 1);
}
inline int32_t sinQ14(int i) { return sinTab[i & (kAngleSteps - 1)]; }
inline int32_t cosQ14(int i) { return sinTab[(i + kAngleSteps / 4) & (kAngleSteps - 1)]; }

inline void setPixel(uint8_t* buf, int x, int y, bool on) {
  uint8_t& b = buf[(y >> 3) * W + x];
  uint8_t bit = 1 << (y & 7);
  if (on) b |= bit;
  else b &= ~bit;
}

// Camera basis for the current frame, shared by walls and sprites.
struct Camera { float dirX, dirY, planeX, planeY, invDet; };
Camera cam;

void setupCamera(float angle) {
  int a = angleIndex(angle);
  cam.dirX = cosQ14(a) / 16384.f;
  cam.dirY = sinQ14(a) / 16384.f;
  cam.planeX = -cam.dirY * (kPlaneQ14 / 16384.f);
  cam.planeY = cam.dirX * (kPlaneQ14 / 16384.f);
  cam.invDet = 1.f / (cam.planeX * cam.dirY - cam.dirX * cam.planeY);
}

// Projects a world point. Returns false if it's behind the camera.
bool project(float px, float py, float wx, float wy, float& screenX, float& depth) {
  float sx = wx - px, sy = wy - py;
  float tx = cam.invDet * (cam.dirY * sx - cam.dirX * sy);
  depth = cam.invDet * (-cam.planeY * sx + cam.planeX * sy);
  if (depth < 0.15f) return false;
  screenX = (W / 2) * (1 + tx / depth);
  return true;
}

}  // namespace

// ---------------------------------------------------------------- game flow

void DoomGame::init() {
  initTables();
  health_ = kStartHealth;
  ammo_ = kStartAmmo;
  score_ = 0;
  loadLevel(0);
}

void DoomGame::loadLevel(int index) {
  level_ = index;
  levelStartScore_ = score_;
  thingCount_ = 0;
  for (float& z : zbuf_) z = 1e9f;
  angle_ = 0;
  for (int y = 0; y < kMapSize; y++) {
    for (int x = 0; x < kMapSize; x++) {
      char c = kLevels[index][y][x];
      map_[y][x] = (c == '#' || c == '%') ? c : '.';
      float cx = x + 0.5f, cy = y + 0.5f;
      if (c == 'P') {
        px_ = cx;
        py_ = cy;
        continue;
      }
      Kind kind;
      switch (c) {
        case 'E': kind = Kind::Enemy; break;
        case 'H': kind = Kind::Medkit; break;
        case 'A': kind = Kind::Ammo; break;
        case 'X': kind = Kind::Exit; break;
        default: continue;
      }
      if (thingCount_ >= kMaxThings) continue;
      Thing& t = things_[thingCount_++];
      memset(&t, 0, sizeof(t));
      t.kind = kind;
      t.alive = true;
      t.hp = 3;
      t.x = cx;
      t.y = cy;
      t.animTimer = random(1000) / 1000.f;
    }
  }
  state_ = State::Playing;
  fireCooldown_ = flashTimer_ = hurtFlash_ = 0;
}

void DoomGame::update(float dt) {
  if (dt > 0.1f) dt = 0.1f;  // don't tunnel through walls after a stall
  fireCooldown_ -= dt;
  flashTimer_ -= dt;
  hurtFlash_ -= dt;

  switch (state_) {
    case State::Playing:
      updatePlayer(dt);
      updateEnemies(dt);
      if (health_ <= 0) {
        health_ = 0;
        state_ = State::Dead;
      }
      break;
    case State::LevelClear:
      stateTimer_ -= dt;
      if (stateTimer_ <= 0) loadLevel(level_ + 1);
      break;
    case State::Dead:
      if (Input::justPressed(BTN_START | BTN_A)) {
        health_ = kStartHealth;
        if (ammo_ < kStartAmmo) ammo_ = kStartAmmo;
        score_ = levelStartScore_;
        loadLevel(level_);
      }
      break;
    case State::Won:
      if (Input::justPressed(BTN_START | BTN_A)) init();
      break;
  }
}

void DoomGame::updatePlayer(float dt) {
  // B held turns left/right into strafing.
  bool strafe = Input::pressed(BTN_B);
  float turn = 0, fwd = 0, side = 0;
  if (Input::pressed(BTN_UP)) fwd += 1;
  if (Input::pressed(BTN_DOWN)) fwd -= 1;
  if (Input::pressed(BTN_LEFT)) (strafe ? side : turn) -= 1;
  if (Input::pressed(BTN_RIGHT)) (strafe ? side : turn) += 1;

  angle_ += turn * kTurnSpeed * dt + Input::look() * kLookRadPerUnit;
  if (angle_ < 0) angle_ += 2 * PI;
  if (angle_ >= 2 * PI) angle_ -= 2 * PI;

  float c = cosf(angle_), s = sinf(angle_);
  float dx = (c * fwd - s * side) * kMoveSpeed * dt;
  float dy = (s * fwd + c * side) * kMoveSpeed * dt;
  tryMove(px_, py_, dx, dy, kPlayerRadius);
  if (fwd != 0 || side != 0) walkPhase_ += dt * 9;

  if (Input::justPressed(BTN_A) || (Input::pressed(BTN_A) && fireCooldown_ <= 0)) shoot();

  for (int i = 0; i < thingCount_; i++) {
    Thing& t = things_[i];
    if (!t.alive || t.kind == Kind::Enemy) continue;
    float d2 = (t.x - px_) * (t.x - px_) + (t.y - py_) * (t.y - py_);
    if (d2 > 0.5f * 0.5f) continue;
    switch (t.kind) {
      case Kind::Medkit:
        if (health_ >= kStartHealth) continue;
        health_ = min(health_ + 25, kStartHealth);
        t.alive = false;
        score_ += 10;
        break;
      case Kind::Ammo:
        ammo_ += 10;
        t.alive = false;
        score_ += 10;
        break;
      case Kind::Exit:
        score_ += 500;
        if (level_ + 1 >= kLevelCount) {
          state_ = State::Won;
        } else {
          state_ = State::LevelClear;
          stateTimer_ = 2.0f;
        }
        return;
      default:
        break;
    }
  }
}

void DoomGame::updateEnemies(float dt) {
  for (int i = 0; i < thingCount_; i++) {
    Thing& t = things_[i];
    if (t.kind != Kind::Enemy || !t.alive) continue;
    t.animTimer += dt;
    t.hurtTimer -= dt;
    t.attackTimer -= dt;
    float dx = px_ - t.x, dy = py_ - t.y;
    float dist = sqrtf(dx * dx + dy * dy);
    if (!t.awake) {
      if (dist < kEnemyWake && lineOfSight(t.x, t.y, px_, py_)) t.awake = true;
      continue;
    }
    if (dist > kEnemyReach) {
      float step = kEnemySpeed * dt / dist;
      tryMove(t.x, t.y, dx * step, dy * step, 0.3f);
    } else if (t.attackTimer <= 0) {
      t.attackTimer = 1.0f;
      health_ -= 6 + random(8);
      hurtFlash_ = 0.12f;
    }
  }
}

void DoomGame::shoot() {
  if (fireCooldown_ > 0 || ammo_ <= 0) return;
  ammo_--;
  fireCooldown_ = kFireDelay;
  flashTimer_ = 0.08f;

  setupCamera(angle_);
  int target = -1;
  float best = 1e9f;
  for (int i = 0; i < thingCount_; i++) {
    Thing& t = things_[i];
    if (t.kind != Kind::Enemy || !t.alive) continue;
    float sx, depth;
    if (!project(px_, py_, t.x, t.y, sx, depth)) continue;
    float halfW = kWallScale / depth * 0.3f + 2;  // body is ~60% of the sprite
    // zbuf_ is from the previous frame; good enough for a one-frame-old wall.
    if (fabsf(sx - W / 2) < halfW && depth < zbuf_[W / 2] + 0.2f && depth < best) {
      best = depth;
      target = i;
    }
  }
  if (target < 0) return;
  Thing& t = things_[target];
  t.awake = true;
  t.hurtTimer = 0.12f;
  t.hp -= best < 2.0f ? 2 : 1;
  if (t.hp <= 0) {
    t.alive = false;
    score_ += 100;
  }
}

bool DoomGame::isWall(float x, float y) const {
  int ix = (int)x, iy = (int)y;
  if (x < 0 || y < 0 || ix >= kMapSize || iy >= kMapSize) return true;
  return map_[iy][ix] != '.';
}

bool DoomGame::lineOfSight(float x0, float y0, float x1, float y1) const {
  float dx = x1 - x0, dy = y1 - y0;
  int steps = (int)(sqrtf(dx * dx + dy * dy) * 4) + 1;
  for (int i = 1; i < steps; i++) {
    float f = (float)i / steps;
    if (isWall(x0 + dx * f, y0 + dy * f)) return false;
  }
  return true;
}

void DoomGame::tryMove(float& x, float& y, float dx, float dy, float r) {
  // Axis-separated so you slide along walls instead of sticking.
  float nx = x + dx;
  float ex = dx > 0 ? r : -r;
  if (!isWall(nx + ex, y - r) && !isWall(nx + ex, y + r)) x = nx;
  float ny = y + dy;
  float ey = dy > 0 ? r : -r;
  if (!isWall(x - r, ny + ey) && !isWall(x + r, ny + ey)) y = ny;
}

// ---------------------------------------------------------------- rendering

void DoomGame::draw(U8G2& g) {
  uint8_t* buf = g.getBufferPtr();
  setupCamera(angle_);
  renderWalls(buf);
  renderThings(buf);
  renderGun(buf);

  if (hurtFlash_ > 0)
    for (int i = 0; i < W * H / 8; i++) buf[i] ^= 0xFF;

  char line[24];
  switch (state_) {
    case State::LevelClear:
      snprintf(line, sizeof(line), "LEVEL %d CLEAR", level_ + 1);
      drawMessage(g, line, nullptr);
      break;
    case State::Dead:
      drawMessage(g, "YOU DIED", "Start: retry");
      break;
    case State::Won:
      drawMessage(g, "YOU WIN!", "Start: again");
      break;
    default:
      break;
  }
}

void DoomGame::renderWalls(uint8_t* buf) {
  const int32_t posX = (int32_t)(px_ * 65536), posY = (int32_t)(py_ * 65536);
  const int a = angleIndex(angle_);
  const int32_t dirX = cosQ14(a), dirY = sinQ14(a);
  const int32_t planeX = -dirY * kPlaneQ14 >> 14, planeY = dirX * kPlaneQ14 >> 14;
  const int mapX0 = posX >> 16, mapY0 = posY >> 16;

  for (int x = 0; x < W; x++) {
    const int32_t rdx = dirX + (planeX * camTab[x] >> 14);
    const int32_t rdy = dirY + (planeY * camTab[x] >> 14);

    // DDA. deltaDist = |1 / rayDir| in Q16; rayDir is Q14 so 2^30 / r.
    const int32_t ddx = rdx ? (int32_t)((1LL << 30) / abs(rdx)) : INT32_MAX / 2;
    const int32_t ddy = rdy ? (int32_t)((1LL << 30) / abs(rdy)) : INT32_MAX / 2;
    int mapX = mapX0, mapY = mapY0;
    int stepX, stepY;
    int64_t sdx, sdy;
    if (rdx < 0) { stepX = -1; sdx = (int64_t)(posX - (mapX << 16)) * ddx >> 16; }
    else         { stepX = 1;  sdx = (int64_t)(((mapX + 1) << 16) - posX) * ddx >> 16; }
    if (rdy < 0) { stepY = -1; sdy = (int64_t)(posY - (mapY << 16)) * ddy >> 16; }
    else         { stepY = 1;  sdy = (int64_t)(((mapY + 1) << 16) - posY) * ddy >> 16; }

    int side = 0;
    char tile = '#';
    for (int i = 0; i < 64; i++) {
      if (sdx < sdy) { sdx += ddx; mapX += stepX; side = 0; }
      else           { sdy += ddy; mapY += stepY; side = 1; }
      if (mapX < 0 || mapY < 0 || mapX >= kMapSize || mapY >= kMapSize) break;
      tile = map_[mapY][mapX];
      if (tile != '.') break;
    }
    int32_t perp = (int32_t)(side == 0 ? sdx - ddx : sdy - ddy);
    if (perp < 1024) perp = 1024;
    zbuf_[x] = perp / 65536.f;

    const int lineH = (kWallScale << 16) / perp;
    const int top = H / 2 - lineH / 2;
    const int bottom = top + lineH;

    // Where along the wall face the ray hit, 0..65535, for edges and bricks.
    int32_t hit = side == 0 ? posY + (int32_t)((int64_t)perp * rdy >> 14)
                            : posX + (int32_t)((int64_t)perp * rdx >> 14);
    const int32_t frac = hit & 0xFFFF;
    const bool edge = frac < 2600 || frac > 65536 - 2600;

    // Brightness 0..16 falls off with distance; y-facing walls are darker.
    int lvl = 16 - (perp >> 15) - (side ? 3 : 0);
    if (lvl < 0) lvl = 0;

    const int y0 = top < 0 ? 0 : top;
    const int y1 = bottom > H ? H : bottom;
    for (int y = y0; y < y1; y++) {
      bool on = kBayer[y & 3][x & 3] < lvl;
      if (tile == '%' && lvl > 2) {
        int texY = (y - top) * 16 / lineH;
        int seam = ((frac >> 12) + ((texY >> 3) & 1) * 8) & 15;
        if ((texY & 7) == 0 || seam == 0) on = false;
      }
      if (edge) on = false;
      if ((y == top || y == bottom - 1) && lvl > 0) on = true;
      if (on) setPixel(buf, x, y, true);
    }
    // Floor: sparse dither that thickens toward the bottom.
    for (int y = (y1 > H / 2 ? y1 : H / 2); y < H; y++)
      if (kBayer[y & 3][x & 3] < ((y - H / 2) >> 3)) setPixel(buf, x, y, true);
  }
}

void DoomGame::renderThings(uint8_t* buf) {
  // Sort far to near so nearer sprites overdraw farther ones.
  struct Vis { int8_t idx; float sx, depth; };
  Vis vis[kMaxThings];
  int n = 0;
  for (int i = 0; i < thingCount_; i++) {
    const Thing& t = things_[i];
    if (!t.alive && t.kind != Kind::Enemy) continue;  // taken pickups vanish
    float sx, depth;
    if (project(px_, py_, t.x, t.y, sx, depth)) vis[n++] = {(int8_t)i, sx, depth};
  }
  for (int i = 1; i < n; i++)
    for (int j = i; j > 0 && vis[j].depth > vis[j - 1].depth; j--) {
      Vis tmp = vis[j]; vis[j] = vis[j - 1]; vis[j - 1] = tmp;
    }

  for (int k = 0; k < n; k++) {
    const Thing& t = things_[vis[k].idx];
    const Art16* a;
    float scale = 0.8f;
    switch (t.kind) {
      case Kind::Enemy:
        if (!t.alive) a = &art[ART_DEAD];
        else if (t.attackTimer > 0.7f) a = &art[ART_ATTACK];
        else a = &art[(t.awake && fmodf(t.animTimer, 0.5f) > 0.25f) ? ART_WALK_B : ART_WALK_A];
        break;
      case Kind::Medkit: a = &art[ART_MEDKIT]; scale = 0.5f; break;
      case Kind::Ammo:   a = &art[ART_AMMO];   scale = 0.5f; break;
      default:           a = &art[ART_EXIT];   scale = 0.9f; break;
    }
    const bool white = t.kind == Kind::Enemy && t.alive && t.hurtTimer > 0;  // hit flash

    const float depth = vis[k].depth;
    const int size = (int)(kWallScale / depth * scale);
    if (size < 2) continue;
    const int floorY = H / 2 + (int)(kWallScale / depth) / 2;
    const int top = floorY - size;
    const int left = (int)vis[k].sx - size / 2;

    for (int sx = 0; sx < size; sx++) {
      int x = left + sx;
      if (x < 0 || x >= W || depth >= zbuf_[x]) continue;
      int tx = sx * 16 / size;
      for (int sy = 0; sy < size; sy++) {
        int y = top + sy;
        if (y < 0 || y >= H) continue;
        int ty = sy * 16 / size;
        if (!(a->mask[ty] >> tx & 1)) continue;
        setPixel(buf, x, y, white || (a->shape[ty] >> tx & 1));
      }
    }
  }
}

void DoomGame::renderGun(uint8_t* buf) {
  int bobX = (int)(sinf(walkPhase_) * 3);
  int bobY = (int)(fabsf(cosf(walkPhase_)) * 2);
  int left = W / 2 - 16 + bobX;
  int top = H - 16 + bobY + (fireCooldown_ > kFireDelay - 0.1f ? 2 : 0);  // recoil
  for (int y = 0; y < 16; y++) {
    int py = top + y;
    if (py < 0 || py >= H) continue;
    for (int x = 0; x < 32; x++) {
      if (!(gunArt.mask[y] >> x & 1)) continue;
      int px = left + x;
      if (px < 0 || px >= W) continue;
      setPixel(buf, px, py, gunArt.shape[y] >> x & 1);
    }
  }
  if (flashTimer_ > 0) {
    int cx = W / 2 + bobX, cy = top - 3;
    for (int dy = -3; dy <= 3; dy++)
      for (int dx = -5; dx <= 5; dx++)
        if (dx * dx + dy * dy * 3 <= 25 && cy + dy >= 0) setPixel(buf, cx + dx, cy + dy, true);
  }
}

HudInfo DoomGame::hud() const {
  HudInfo h;
  h.health = health_;
  h.ammo = ammo_;
  h.score = score_;
  h.level = level_ + 1;
  return h;
}
