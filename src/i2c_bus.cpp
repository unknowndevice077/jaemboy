#include "i2c_bus.h"
#include <Wire.h>
#include "config.h"

namespace {
  constexpr uint8_t NONE = 0xFF;  // cached state unknown
  uint8_t current = NONE;

  bool writeMux(uint8_t mask) {
    Wire.beginTransmission(MUX_ADDR);
    Wire.write(mask);
    uint8_t err = Wire.endTransmission();
    if (err) Serial.printf("[i2c] mux write 0x%02X failed: %s\n", mask, I2CBus::errStr(err));
    return err == 0;
  }

  bool probe(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
  }
}

namespace I2CBus {

const char* errStr(uint8_t code) {
  switch (code) {
    case 0: return "ok";
    case 1: return "data too long for Wire buffer";
    case 2: return "NACK on address (device missing / wrong address / mux channel off)";
    case 3: return "NACK on data";
    case 4: return "other error (bus stuck? check pull-ups and wiring)";
    case 5: return "timeout (SDA/SCL held low? check wiring and pull-ups)";
    default: return "unknown";
  }
}

void begin() {
  // A full SH1106 page (control bytes + 128 data bytes) must fit in one
  // transaction; the default 128-byte Wire buffer is too small.
  size_t buf = Wire.setBufferSize(256);
  Serial.printf("[i2c] Wire buffer: %u bytes%s\n", (unsigned)buf,
                buf < 135 ? "  !! too small, flush will fail" : "");
  bool ok = Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_CLOCK_HZ);
  Serial.printf("[i2c] Wire.begin(SDA=%d, SCL=%d, %lu Hz): %s\n", PIN_I2C_SDA,
                PIN_I2C_SCL, (unsigned long)I2C_CLOCK_HZ, ok ? "ok" : "FAILED");
  Serial.printf("[i2c] actual clock: %lu Hz\n", (unsigned long)Wire.getClock());
  Wire.setTimeOut(20);
  current = NONE;
}

bool select(uint8_t channel) {
  if (channel == current) return true;
  if (!writeMux(1u << channel)) {
    Serial.printf("[i2c] select channel %u failed\n", channel);
    current = NONE;
    return false;
  }
  current = channel;
  return true;
}

bool deselectAll() {
  bool ok = writeMux(0);
  current = ok ? 0xFE : NONE;  // 0xFE = "no channel", distinct from any 0..7
  return ok;
}

void scan() {
  Serial.println(F("\n=== I2C scan ==="));
  Serial.printf("SDA=%d SCL=%d clock=%lu Hz\n", PIN_I2C_SDA, PIN_I2C_SCL,
                (unsigned long)I2C_CLOCK_HZ);

  if (!deselectAll()) {
    Serial.printf("!! Mux not responding at 0x%02X. Check wiring, A0-A2 to GND, "
                  "RESET pulled high.\n", MUX_ADDR);
  }
  Serial.print(F("Main bus (all channels off):"));
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    if (probe(a)) { Serial.printf(" 0x%02X", a); found++; }
  }
  Serial.println(found ? "" : " none");

  for (uint8_t ch = 0; ch < 8; ch++) {
    if (!select(ch)) {
      Serial.printf("Channel %u: select failed\n", ch);
      continue;
    }
    Serial.printf("Channel %u:", ch);
    found = 0;
    for (uint8_t a = 1; a < 127; a++) {
      if (a == MUX_ADDR) continue;  // the mux itself answers on every channel
      if (probe(a)) { Serial.printf(" 0x%02X", a); found++; }
    }
    Serial.println(found ? "" : " none");
  }
  deselectAll();
  Serial.println(F("=== scan done ===\n"));
}

}  // namespace I2CBus
