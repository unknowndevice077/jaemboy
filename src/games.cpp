#include "game.h"
#include "arcade.h"
#include "config.h"
#include "doom.h"
#include "snake.h"

namespace {
DoomGame doom;
JumperGame jumper;
BlocksGame blocks;
BricksGame bricks;
FlapGame flap;
SnakeGame snake;
}  // namespace

Game* const kGames[] = {&doom, &jumper, &blocks, &bricks, &flap, &snake};
const uint8_t kGameCount = sizeof(kGames) / sizeof(kGames[0]);

void drawMessage(U8G2& g, const char* line1, const char* line2) {
  g.setFont(u8g2_font_6x10_tf);
  int h = line2 ? 26 : 15;
  int y = (OLED_HEIGHT - h) / 2;
  g.setDrawColor(0);
  g.drawBox(14, y, OLED_WIDTH - 28, h);
  g.setDrawColor(1);
  g.drawFrame(14, y, OLED_WIDTH - 28, h);
  g.drawStr((OLED_WIDTH - g.getStrWidth(line1)) / 2, y + 11, line1);
  if (line2) g.drawStr((OLED_WIDTH - g.getStrWidth(line2)) / 2, y + 22, line2);
}
