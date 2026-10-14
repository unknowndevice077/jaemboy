#include "oled.h"
#include <Wire.h>
#include "i2c_bus.h"

Oled::Oled(uint8_t muxChannel)
    : u8g2_(U8G2_R0, U8X8_PIN_NONE), channel_(muxChannel) {}

bool Oled::begin() {
  if (!I2CBus::select(channel_)) return false;
  Wire.beginTransmission(OLED_I2C_ADDR);
  uint8_t err = Wire.endTransmission();
  if (err) {
    Serial.printf("[oled ch%u] no reply at 0x%02X: %s\n", channel_, OLED_I2C_ADDR,
                  I2CBus::errStr(err));
    return false;
  }
  Serial.printf("[oled ch%u] found at 0x%02X, sending init\n", channel_, OLED_I2C_ADDR);
  u8g2_.setI2CAddress(OLED_I2C_ADDR << 1);  // U8g2 wants the 8-bit form
  u8g2_.setBusClock(I2C_CLOCK_HZ);
  if (!u8g2_.begin()) {
    Serial.printf("[oled ch%u] U8g2 begin failed\n", channel_);
    return false;
  }
  u8g2_.clearBuffer();
  return flush();
}

bool Oled::flush() {
  if (writePages()) return true;
  // A write failed. Log the mux state, free the bus in case the panel is
  // stuck mid-transfer, re-init the panel and retry once.
  int reg = I2CBus::readMux();
  Serial.printf("[oled ch%u] flush failed, mux register = 0x%02X%s\n", channel_, reg,
                reg < 0 ? " (read failed)" : "");
  I2CBus::clearBus();
  I2CBus::invalidate();
  if (!I2CBus::select(channel_)) return false;
  Wire.beginTransmission(OLED_I2C_ADDR);
  uint8_t err = Wire.endTransmission();
  Serial.printf("[oled ch%u] after bus clear: %s\n", channel_, I2CBus::errStr(err));
  if (err) return false;
  u8g2_.initDisplay();  // the panel may have lost its state; resend init
  u8g2_.setPowerSave(0);
  bool ok = writePages();
  Serial.printf("[oled ch%u] retry %s\n", channel_, ok ? "ok" : "FAILED");
  return ok;
}

bool Oled::writePages() {
  if (!I2CBus::select(channel_)) return false;
  const uint8_t* buf = u8g2_.getBufferPtr();
  constexpr uint8_t col = OLED_COL_OFFSET;
  bool ok = true;

  for (uint8_t page = 0; page < OLED_HEIGHT / 8; page++) {
    Wire.beginTransmission(OLED_I2C_ADDR);
    // 0x80 = "one command byte follows, more control bytes after it".
    // Lets us set the page/column and stream data in a single transaction.
    Wire.write(0x80); Wire.write(0xB0 | page);
    Wire.write(0x80); Wire.write(0x00 | (col & 0x0F));
    Wire.write(0x80); Wire.write(0x10 | (col >> 4));
    Wire.write(0x40);  // the rest is display data
    Wire.write(buf + page * OLED_WIDTH, OLED_WIDTH);
    uint8_t err = Wire.endTransmission();
    if (err) {
      Serial.printf("[oled ch%u] page %u write failed: %s\n", channel_, page,
                    I2CBus::errStr(err));
      ok = false;
    }
  }
  return ok;
}
