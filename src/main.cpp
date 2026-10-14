#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "game.h"
#include "i2c_bus.h"
#include "input.h"
#include "net.h"
#include "oled.h"
#include "status_screen.h"

Oled oledStatus(MUX_CH_LAUNCH);
Oled oledGame(MUX_CH_GAME);

namespace {

Game* running = nullptr;
StatusModel model;

uint32_t lastFrameUs;
uint32_t fpsWindowStart;
uint16_t fpsFrames;

void launch(uint8_t index) {
  running = kGames[index];
  Serial.printf("[app] launch %s\n", running->name());
  running->init();
  model.inGame = true;
  fpsWindowStart = millis();
  fpsFrames = 0;
  model.fps = DEBUG_FPS ? 0 : -1;
}

void quit() {
  Serial.printf("[app] exit %s\n", running->name());
  running->exit();
  running = nullptr;
  model.inGame = false;
  model.hud = HudInfo();
  model.fps = -1;
  oledGame.gfx().clearBuffer();
  oledGame.flush();
}

void launcherTick() {
  if (Input::justPressed(BTN_UP)) model.cursor = (model.cursor + kGameCount - 1) % kGameCount;
  if (Input::justPressed(BTN_DOWN)) model.cursor = (model.cursor + 1) % kGameCount;
  if (Input::justPressed(BTN_START)) launch(model.cursor);
}

void gameTick(float dt) {
  if (Input::justPressed(BTN_SELECT)) {
    quit();
    return;
  }
  uint32_t t0 = micros();
  running->update(dt);
  U8G2& g = oledGame.gfx();
  g.clearBuffer();
  running->draw(g);
  uint32_t t1 = micros();
  oledGame.flush();
  uint32_t t2 = micros();
  model.hud = running->hud();

#if DEBUG_FPS
  static uint32_t drawUs, flushUs;
  drawUs += t1 - t0;
  flushUs += t2 - t1;
  fpsFrames++;
  uint32_t now = millis();
  if (now - fpsWindowStart >= 1000) {
    model.fps = fpsFrames * 1000 / (now - fpsWindowStart);
    Serial.printf("[perf] %s: %d fps, update+draw %lu us, flush %lu us per frame\n",
                  running->name(), model.fps, (unsigned long)(drawUs / fpsFrames),
                  (unsigned long)(flushUs / fpsFrames));
    fpsWindowStart = now;
    fpsFrames = 0;
    drawUs = flushUs = 0;
  }
#else
  (void)t0; (void)t1; (void)t2;
#endif
}

// Debug: serial 'c' steps the I2C clock so speeds can be compared without
// reflashing. Set the one that works in config.h (I2C_CLOCK_HZ).
void cycleI2cClock() {
  static const uint32_t kClocks[] = {400000, 800000, 1000000};
  static uint8_t i = I2C_CLOCK_HZ >= 1000000 ? 2 : I2C_CLOCK_HZ >= 800000 ? 1 : 0;
  i = (i + 1) % 3;
  Wire.setClock(kClocks[i]);
  Serial.printf("[i2c] clock -> %lu Hz (actual %lu)\n", (unsigned long)kClocks[i],
                (unsigned long)Wire.getClock());
}

void frame() {
  uint32_t now = micros();
  float dt = (now - lastFrameUs) * 1e-6f;
  lastFrameUs = now;

  if (Input::pollSerial() == 'c') cycleI2cClock();
  Input::update();

  if (running) gameTick(dt);
  else launcherTick();

  model.apUp = Net::apUp();
  model.clients = Net::clients();
  if (StatusScreen::show(model))
    Serial.printf("[status] redraw: %s cursor=%u fps=%d pads=%u hp=%d ammo=%d score=%d lv=%d\n",
                  model.inGame ? "hud" : "menu", model.cursor, model.fps, model.clients,
                  model.hud.health, model.hud.ammo, model.hud.score, model.hud.level);
}

void gameTask(void*) {
  lastFrameUs = micros();
  for (;;) {
    frame();
    // The launcher has nothing to animate. In game, still yield a tick so
    // lower-priority tasks on this core get to run.
    vTaskDelay(pdMS_TO_TICKS(running ? 1 : 10));
  }
}

}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);
  Serial.println(F("\nGAMEBOY"));
  Serial.printf("[sys] reset reason %d, %u MHz, free heap %u\n", esp_reset_reason(),
                ESP.getCpuFreqMHz(), ESP.getFreeHeap());

  I2CBus::begin();
  I2CBus::scan();
  if (!oledStatus.begin()) Serial.println(F("[sys] OLED 1 init FAILED"));
  if (!oledGame.begin()) Serial.println(F("[sys] OLED 2 init FAILED"));
  StatusScreen::begin(oledStatus);

  Net::begin();
#if SERIAL_INPUT
  Serial.println(F("[sys] serial input: w/a/s/d, j=A, k=B, Enter=Start, Backspace=Select"));
#endif

  xTaskCreatePinnedToCore(gameTask, "game", 8192, nullptr, 2, nullptr, GAME_CORE);
}

void loop() {
  vTaskDelete(nullptr);  // everything runs in gameTask and the network tasks
}
