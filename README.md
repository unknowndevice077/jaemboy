# jaemboy — ESP32 handheld

ESP32 devkit + TCA9548A I2C mux + two SH1106 128x64 OLEDs. Controlled from a phone over WiFi.

## Wiring

| From            | To                         |
|-----------------|----------------------------|
| ESP32 GPIO21    | TCA9548A SDA               |
| ESP32 GPIO22    | TCA9548A SCL               |
| ESP32 3V3       | TCA9548A VIN, both OLED VCC |
| ESP32 GND       | TCA9548A GND, A0, A1, A2, both OLED GND |
| TCA9548A SD0/SC0 | OLED 1 (launcher/HUD) SDA/SCL |
| TCA9548A SD1/SC1 | OLED 2 (game) SDA/SCL      |
| 2S LiPo + (via switch) | ESP32 VIN           |
| 2S LiPo −       | ESP32 GND                  |

- Mux address is 0x70 (A0–A2 to GND). If the breakout's RESET pin isn't pulled high on the board, tie it to 3V3.
- Both OLEDs stay at 0x3C; the mux keeps them on separate buses.
- I2C needs pull-ups on the main bus and on each mux channel used. Most breakouts have them; at 800 kHz–1 MHz stronger pull-ups (2.2k–4.7k) help.
- Power: 2S LiPo is 6.0–8.4 V into the devkit's onboard 3.3 V regulator. It works, but the regulator runs warm with WiFi on.

## Configuration

Everything hardware-specific is in `include/config.h`: pins, I2C clock, mux channels, OLED driver/size/column offset.

## Build / upload

Requires [PlatformIO](https://platformio.org/) (VS Code extension or `pip install platformio`).

```sh
pio run                 # build
pio run -t upload       # flash (hold BOOT if the board doesn't auto-reset)
pio device monitor      # serial at 115200
```

## Phase 1 check

After flashing, the serial monitor prints the I2C scan. Expected:

```
Main bus (all channels off): 0x70
Channel 0: 0x3C
Channel 1: 0x3C
Channel 2..7: none
```

OLED 1 shows `OLED 1 LAUNCH / mux ch 0`, and 1.5 s later OLED 2 shows `OLED 2 GAME / mux ch 1`. Both have a frame around the full edge. If the left or right edge line is missing, adjust `OLED_COL_OFFSET`. Send `s` in the monitor to rescan.
