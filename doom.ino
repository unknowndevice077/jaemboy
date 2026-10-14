#include <Wire.h>
#include <U8g2lib.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
uint8_t buffer[1024];

const int PINS[] = {32, 27, 5, 18}; 
const char CMDS[] = {'U', 'D', 'M', 'C'}; 

void select(uint8_t i) {
  Wire.beginTransmission(0x70);
  Wire.write(1 << i);
  Wire.endTransmission();
}

void setup() {
  Serial.begin(115200);
  for(int p : PINS) pinMode(p, INPUT_PULLUP);
  Wire.begin(21, 22);
  Wire.setClock(400000); 
  
  for(int i=0; i<2; i++){
    select(i);
    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(15, 35, "RJ-OS READY");
    u8g2.sendBuffer();
  }
  Serial.print('A'); // Initial "Ready" signal
}

void loop() {
  static unsigned long lastBtn = 0;
  if (millis() - lastBtn > 150) { 
    for(int i=0; i<4; i++) {
      if (digitalRead(PINS[i]) == LOW) {
        Serial.print(CMDS[i]);
        lastBtn = millis();
        break; 
      }
    }
  }

  if (Serial.available() >= 1026) {
    if (Serial.read() == 0xFE) {
      uint8_t ch = Serial.read();
      if (Serial.readBytes((char*)buffer, 1024) == 1024) {
        select(ch);
        u8g2.drawBitmap(0, 0, 16, 64, buffer);
        u8g2.sendBuffer();
        Serial.print('A'); // Tell Python to send next frame
      }
    }
  }
}