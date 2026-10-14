#include "status_screen.h"
#include "config.h"
#include "net.h"

namespace {
  Oled* oled = nullptr;
  StatusModel last;
  bool drawn = false;

  constexpr int kRowH = 11;
  constexpr int kListTop = 13;
  constexpr int kVisibleRows = 3;  // between the title bar and the footer

  void titleBar(U8G2& g, const char* text) {
    g.setDrawColor(1);
    g.drawBox(0, 0, OLED_WIDTH, kRowH);
    g.setDrawColor(0);
    g.drawStr(2, 9, text);
    g.setDrawColor(1);
  }

  void footer(U8G2& g, const StatusModel& m) {
    char line[32];
    if (m.apUp)
      snprintf(line, sizeof(line), "%s pad:%s", Net::ip(), m.clients ? "ok" : "--");
    else
      snprintf(line, sizeof(line), "input: serial");
    g.drawHLine(0, OLED_HEIGHT - 11, OLED_WIDTH);
    g.drawStr(2, OLED_HEIGHT - 1, line);
  }

  void drawLauncher(U8G2& g, const StatusModel& m) {
    titleBar(g, "GAMEBOY");
    int first = m.cursor >= kVisibleRows ? m.cursor - kVisibleRows + 1 : 0;
    for (int row = 0; row < kVisibleRows && first + row < kGameCount; row++) {
      int i = first + row;
      int y = kListTop + row * kRowH;
      if (i == m.cursor) {
        g.drawBox(0, y - 1, OLED_WIDTH, kRowH);
        g.setDrawColor(0);
      }
      g.drawStr(4, y + 8, i == m.cursor ? ">" : " ");
      g.drawStr(12, y + 8, kGames[i]->name());
      g.setDrawColor(1);
    }
    footer(g, m);
  }

  void drawHud(U8G2& g, const StatusModel& m) {
    titleBar(g, kGames[m.cursor]->name());
    char line[32];
    int y = kListTop + 8;
    const HudInfo& h = m.hud;
    if (h.health >= 0 || h.ammo >= 0 || h.lives >= 0) {
      line[0] = 0;
      int n = 0;
      if (h.health >= 0) n += snprintf(line + n, sizeof(line) - n, "HP %d  ", h.health);
      if (h.ammo >= 0) n += snprintf(line + n, sizeof(line) - n, "AMMO %d  ", h.ammo);
      if (h.lives >= 0) snprintf(line + n, sizeof(line) - n, "LIVES %d", h.lives);
      g.drawStr(2, y, line);
      y += kRowH;
    }
    if (h.score >= 0 || h.level >= 0) {
      line[0] = 0;
      int n = 0;
      if (h.score >= 0) n += snprintf(line + n, sizeof(line) - n, "SCORE %d  ", h.score);
      if (h.level >= 0) snprintf(line + n, sizeof(line) - n, "LV %d", h.level);
      g.drawStr(2, y, line);
      y += kRowH;
    }
    if (m.fps >= 0) {
      snprintf(line, sizeof(line), "FPS %d", m.fps);
      g.drawStr(2, y, line);
    }
    footer(g, m);
  }
}

namespace StatusScreen {

void begin(Oled& o) {
  oled = &o;
  drawn = false;
}

bool show(const StatusModel& m) {
  if (drawn && m == last) return false;
  U8G2& g = oled->gfx();
  g.clearBuffer();
  g.setFont(u8g2_font_6x10_tf);
  if (m.inGame) drawHud(g, m);
  else drawLauncher(g, m);
  oled->flush();
  last = m;
  drawn = true;
  return true;
}

}  // namespace StatusScreen
