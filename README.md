# jaemboy — ESP32 handheld

ESP32 devkit + TCA9548A I2C mux + two SH1106 128x64 OLEDs, controlled from a phone over WiFi.

- **OLED 1** (mux channel 0): game list, then HUD (HP / ammo / lives / score / level, FPS, IP, pad status).
- **OLED 2** (mux channel 1): the game.

## Wiring

| From            | To                         |
|-----------------|----------------------------|
| ESP32 GPIO21    | TCA9548A SDA               |
| ESP32 GPIO22    | TCA9548A SCL               |
| ESP32 3V3       | TCA9548A VIN, both OLED VDD |
| ESP32 GND       | TCA9548A GND, A0, A1, A2, both OLED GND |
| TCA9548A SD0/SC0 | OLED 1 (launcher/HUD) SDA/SCK |
| TCA9548A SD1/SC1 | OLED 2 (game) SDA/SCK      |
| 2S LiPo + (via switch) | ESP32 VIN           |
| 2S LiPo −       | ESP32 GND                  |

- Mux address 0x70 (A0–A2 to GND). Tie the mux RESET pin to 3V3 if the breakout doesn't pull it up.
- Both OLEDs stay at 0x3C; the mux keeps them on separate buses.
- Use one solid jumper per connection. Chained jumpers caused dropouts and garbled pixels during bring-up.
- Keep the LiPo switch off while the board is on USB.

## Build / upload

Requires [PlatformIO](https://platformio.org/) (VS Code extension or `pip install platformio`).

```sh
pio run                 # build
pio run -t upload       # flash
pio device monitor      # serial at 115200
```

This devkit doesn't enter download mode by itself: hold **BOOT** until the upload shows `Writing at ...`. A 10 µF capacitor from EN to GND usually fixes auto-reset.

The controller page is `data/controller.html`. A pre-build script (`scripts/embed_web.py`) gzips it into flash, so there is no separate filesystem upload.

## Playing

1. Power on. OLED 1 shows the game list and `192.168.4.1 pad:--`.
2. On the phone, join WiFi **GAMEBOY** (password **gameboy123**) and open **http://192.168.4.1** in landscape.
3. D-pad picks a game, **Start** launches, **Select** returns to the list.

The phone page: D-pad, A, B, Start, Select (multitouch), and dragging on empty space turns the view in Doom. **EDIT** lets you drag controls, resize them (− / +), and set look sensitivity; the layout is saved on the phone.

| Game | Controls |
|------|----------|
| Doom (raycaster) | Up/Down walk, Left/Right turn (or swipe), A fire, hold B + Left/Right strafe |
| Jumper (platformer) | Left/Right run, A jump (hold = higher), hold B run faster |
| Blocks (falling blocks) | Left/Right move, Down soft drop, Up hard drop, A/B rotate |
| Bricks (breakout) | Left/Right paddle, A launch |
| Flap | A flap |
| Snake | D-pad |

### Serial input (debug)

With `SERIAL_INPUT 1` in `config.h`, serial keys act as buttons: `w a s d` D-pad, `j` A, `k` B, Enter Start, Backspace Select. `c` cycles the I2C clock (400k / 800k / 1M) for testing.

## Configuration

`include/config.h`: pins, I2C clock, mux channels, OLED driver/size/column offset (one block), WiFi SSID/password, debug flags (`DEBUG_FPS`, `SERIAL_INPUT`).

## Architecture

- `src/main.cpp`: game loop in a FreeRTOS task on core 1 (the only task that touches I2C). WiFi and the async web server run on core 0.
- `src/input.cpp`: one input API for games: `pressed()`, `justPressed()`, `justReleased()`, `look()`. Presses are latched between frames, so short taps aren't lost. Buttons are released when the WebSocket disconnects.
- `src/net.cpp`: WiFi AP, controller page, WebSocket (`/ws`). Message: `[button mask, look delta]`.
- `src/oled.cpp`: U8g2 for init and drawing; own flush (one I2C transaction per page, mux channel cached so it's only re-selected when it changes).
- `src/status_screen.cpp`: OLED 1, redrawn only when its content changes.
- Each game implements `init()`, `update(dt)`, `draw()`, `exit()` (`include/game.h`), registered in `src/games.cpp`.

## Performance (measured)

Full 128x64 frame to OLED 2:

| I2C clock | Flush | Doom FPS |
|-----------|-------|----------|
| 400 kHz   | 27.9 ms | 31 |
| 800 kHz   | 15.7 ms | 55 |
| 1 MHz (default) | 13.2 ms | 60 |

The raycaster (fixed-point DDA, 128 columns, sprites) takes ~1.5 ms per frame; the rest is I2C. If the screens show garbage at 1 MHz, lower `I2C_CLOCK_HZ`.

## Licenses and assets

- All game code, levels and sprite art are original. The Doom-style game follows the general approach of [doom-nano](https://github.com/daveruiz/doom-nano) (MIT): grid map, DDA raycasting, billboard sprites. No code or assets were copied from it, and no WAD or id Software assets are included. The other games are original takes on classic genres, with no Nintendo or other third-party assets.
- Libraries: [U8g2](https://github.com/olikraus/u8g2) (BSD-2-Clause), [ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer) and [AsyncTCP](https://github.com/ESP32Async/AsyncTCP) (LGPL-3.0, linked as libraries).
