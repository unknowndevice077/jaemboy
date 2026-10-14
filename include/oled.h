#pragma once
#include <U8g2lib.h>
#include "config.h"

// One OLED behind one mux channel. Draw with gfx(), then flush().
class Oled {
 public:
  explicit Oled(uint8_t muxChannel);
  bool begin();
  U8G2& gfx() { return u8g2_; }
  // Sends the whole framebuffer: one mux select (if needed) and one I2C
  // transaction per 8-pixel page.
  bool flush();
  uint8_t channel() const { return channel_; }

 private:
  bool writePages();
  OLED_U8G2_CLASS u8g2_;
  uint8_t channel_;
};
