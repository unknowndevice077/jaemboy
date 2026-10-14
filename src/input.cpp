#include "input.h"
#include "config.h"

namespace {
  portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
  uint8_t sources[Input::SRC_COUNT];
  uint8_t raw;           // OR of all sources
  uint8_t downLatch;     // edges since the last update()
  uint8_t upLatch;

  int32_t lookAccum;

  uint8_t cur, down, up;  // snapshot for the current frame (game task only)
  int lookNow;

  uint8_t keyToButton(char c) {
    switch (c) {
      case 'w': return BTN_UP;
      case 's': return BTN_DOWN;
      case 'a': return BTN_LEFT;
      case 'd': return BTN_RIGHT;
      case 'j': return BTN_A;
      case 'k': return BTN_B;
      case '\r': case '\n': return BTN_START;
      case 8: case 127: return BTN_SELECT;  // Backspace / Delete
      default: return 0;
    }
  }
}

namespace Input {

void setSource(Source src, uint8_t mask) {
  portENTER_CRITICAL(&lock);
  sources[src] = mask;
  uint8_t next = 0;
  for (uint8_t s : sources) next |= s;
  downLatch |= next & ~raw;
  upLatch |= raw & ~next;
  raw = next;
  portEXIT_CRITICAL(&lock);
}

void update() {
  portENTER_CRITICAL(&lock);
  cur = raw;
  down = downLatch;
  up = upLatch;
  downLatch = upLatch = 0;
  lookNow = lookAccum;
  lookAccum = 0;
  portEXIT_CRITICAL(&lock);
}

void addLook(int delta) {
  portENTER_CRITICAL(&lock);
  lookAccum += delta;
  portEXIT_CRITICAL(&lock);
}

int look() { return lookNow; }

bool pressed(uint8_t b) { return cur & b; }
bool justPressed(uint8_t b) { return down & b; }
bool justReleased(uint8_t b) { return up & b; }

char pollSerial() {
  char other = 0;
#if SERIAL_INPUT
  static uint32_t releaseAt[8];
  static uint8_t mask;
  static char last;
  uint32_t now = millis();
  uint8_t next = mask;

  while (Serial.available()) {
    char c = Serial.read();
    bool crlf = (c == '\n' && last == '\r');  // one Enter, not two
    last = c;
    if (crlf) continue;
    uint8_t b = keyToButton(c);
    if (!b) {
      other = c;
      continue;
    }
    next |= b;
    releaseAt[__builtin_ctz(b)] = now + SERIAL_HOLD_MS;
  }
  for (int i = 0; i < 8; i++)
    if ((next & (1 << i)) && (int32_t)(now - releaseAt[i]) >= 0) next &= ~(1 << i);

  if (next != mask) {
    mask = next;
    setSource(SRC_SERIAL, mask);
  }
#endif
  return other;
}

}  // namespace Input
