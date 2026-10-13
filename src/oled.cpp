#include "oled.h"
#include <Wire.h>
#include "i2c_bus.h"

Oled::Oled(uint8_t muxChannel)
    : u8g2_(U8G2_R0, U8X8_PIN_NONE), channel_(muxChannel) {}

bool Oled::begin() {
  if (!I2CBus::select(channel_)) return false;
  u8g2_.setI2CAddress(OLED_I2C_ADDR << 1);  // U8g2 wants the 8-bit form
  u8g2_.setBusClock(I2C_CLOCK_HZ);
  if (!u8g2_.begin()) return false;
  u8g2_.clearBuffer();
  return flush();
}

bool Oled::flush() {
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
    if (Wire.endTransmission() != 0) ok = false;
  }
  return ok;
}
