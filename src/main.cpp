// Phase 1: hardware bring-up. I2C scan through the mux, then a test pattern
// on each OLED separately so you can confirm which channel drives which panel.
#include <Arduino.h>
#include "config.h"
#include "i2c_bus.h"
#include "oled.h"

Oled oledLaunch(MUX_CH_LAUNCH);
Oled oledGame(MUX_CH_GAME);

static void drawTestPattern(Oled& o, const char* name) {
  U8G2& g = o.gfx();
  g.clearBuffer();
  // Frame touching all four edges: a missing column on the left/right means
  // OLED_COL_OFFSET is wrong.
  g.drawFrame(0, 0, OLED_WIDTH, OLED_HEIGHT);
  g.setFont(u8g2_font_6x10_tf);
  g.drawStr(4, 12, name);
  char line[24];
  snprintf(line, sizeof(line), "mux ch %u", o.channel());
  g.drawStr(4, 24, line);
  // Checkerboard in the lower right to spot shifted/garbled pages.
  for (int y = OLED_HEIGHT / 2; y < OLED_HEIGHT - 2; y += 4)
    for (int x = OLED_WIDTH / 2; x < OLED_WIDTH - 2; x += 4)
      if (((x + y) / 4) & 1) g.drawBox(x, y, 4, 4);
  // Diagonal from the bottom left.
  g.drawLine(2, OLED_HEIGHT - 3, OLED_WIDTH / 2 - 4, OLED_HEIGHT / 2);
}

static void bringUp(Oled& o, const char* name) {
  Serial.printf("%s (ch %u): ", name, o.channel());
  if (!o.begin()) {
    Serial.println(F("begin FAILED"));
    return;
  }
  drawTestPattern(o, name);
  uint32_t t0 = micros();
  bool ok = o.flush();
  uint32_t dt = micros() - t0;
  Serial.printf("%s, flush %lu us\n", ok ? "OK" : "flush FAILED", (unsigned long)dt);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);
  Serial.println(F("\nGAMEBOY phase 1: hardware bring-up"));
  Serial.printf("[sys] chip %s rev %u, %u MHz, free heap %u\n", ESP.getChipModel(),
                ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getFreeHeap());
  Serial.printf("[cfg] OLED %dx%d, col offset %d, addr 0x%02X, mux 0x%02X ch %d/%d\n",
                OLED_WIDTH, OLED_HEIGHT, OLED_COL_OFFSET, OLED_I2C_ADDR, MUX_ADDR,
                MUX_CH_LAUNCH, MUX_CH_GAME);

  I2CBus::begin();
  I2CBus::scan();

  // Init each panel with the other one still blank so a wrong channel is obvious.
  bringUp(oledLaunch, "OLED 1 LAUNCH");
  delay(1500);
  bringUp(oledGame, "OLED 2 GAME");

  Serial.println(F("Expected: OLED 1 shows 'OLED 1 LAUNCH / mux ch 0',"));
  Serial.println(F("          OLED 2 shows 'OLED 2 GAME / mux ch 1'."));
  Serial.println(F("Send 's' to rescan."));
  Serial.println(F("[sys] setup done"));
}

void loop() {
  if (Serial.available() && Serial.read() == 's') I2CBus::scan();
  delay(10);
}
