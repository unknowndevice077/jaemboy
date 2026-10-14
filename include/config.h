#pragma once
#include <Arduino.h>

// ---------- I2C bus ----------
#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22

// Measured (Phase 6): 400k -> 31 fps, 800k -> 55 fps, 1M -> 62 fps in Doom.
// If OLEDs show garbage, drop to 800000 or 400000 (depends on pull-ups and
// wire length).
#define I2C_CLOCK_HZ 1000000

// ---------- TCA9548A mux ----------
#define MUX_ADDR      0x70
#define MUX_CH_LAUNCH 0  // OLED 1: launcher / HUD
#define MUX_CH_GAME   1  // OLED 2: game viewport

// ---------- OLED (one block configures both screens) ----------
// Pick the U8g2 *_F_HW_I2C class that matches the panel. U8g2 is used for the
// init sequence and drawing; flushing is done by our own fast page writer.
//   SH1106 128x64 (1.3")  : U8G2_SH1106_128X64_NONAME_F_HW_I2C, offset 2
//   SSD1306 128x64 (0.96"): U8G2_SSD1306_128X64_NONAME_F_HW_I2C, offset 0
//   SSD1306 64x48 (0.66") : U8G2_SSD1306_64X48_ER_F_HW_I2C,      offset 32
//   SSD1306 64x32 (0.49") : U8G2_SSD1306_64X32_NONAME_F_HW_I2C,  offset 32
#define OLED_U8G2_CLASS  U8G2_SH1106_128X64_NONAME_F_HW_I2C
#define OLED_WIDTH       128
#define OLED_HEIGHT      64
#define OLED_COL_OFFSET  2     // SH1106 RAM is 132 wide; visible area starts at col 2
#define OLED_I2C_ADDR    0x3C  // 7-bit

// ---------- WiFi access point / controller ----------
#define AP_SSID     "GAMEBOY"
#define AP_PASSWORD "gameboy123"  // 8+ chars for WPA2; "" = open network
#define AP_CHANNEL  6
#define AP_MAX_CONN 2

// ---------- Cores ----------
// WiFi and the async web server run on core 0; the game loop owns core 1
// (and is the only task that touches I2C).
#define GAME_CORE 1

// ---------- Debug ----------
#define SERIAL_BAUD 115200
// Shows FPS on OLED 1 while a game runs.
#define DEBUG_FPS 1
// Serial keys act as buttons: w/a/s/d = D-pad, j = A, k = B,
// Enter = Start, Backspace = Select.
#define SERIAL_INPUT 1
// A serial key holds its button this long (terminals send repeats while a
// key is held, so this bridges the gaps).
#define SERIAL_HOLD_MS 200
