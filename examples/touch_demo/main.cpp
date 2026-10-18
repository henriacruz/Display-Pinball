#include <Arduino.h>
#include <Preferences.h>
#include "Display.hpp"

namespace {
Display display;
Preferences preferences;
uint32_t touches = 0;
bool wasTouched = false;

void drawScreen() {
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setTextSize(2);
  display.setCursor(20, 24);
  display.println("DISPLAY PINBALL");
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(20, 65);
  display.println("ESP32 / 320 x 480");
  display.setCursor(20, 100);
  display.println("Toque para desenhar");
  display.drawRect(10, 140, 300, 260, TFT_DARKGREY);
  display.fillRoundRect(20, 420, 280, 40, 8, TFT_BLUE);
  display.setTextColor(TFT_WHITE, TFT_BLUE);
  display.setCursor(124, 433);
  display.print("LIMPAR");
}

void calibrate() {
  uint16_t calibration[8];
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(15, 200);
  display.println("Toque nas setas");
  Serial.println("Calibracao: toque nas setas ate concluir.");
  display.calibrateTouch(calibration, TFT_WHITE, TFT_BLACK, 15);
  if (preferences.begin("display", false)) {
    const size_t saved = preferences.putBytes("touch-v1", calibration, sizeof(calibration));
    preferences.end();
    Serial.println(saved == sizeof(calibration) ? "Calibracao salva." : "Falha ao salvar calibracao.");
  }
  display.setTouchCalibrate(calibration);
  wasTouched = true;
  drawScreen();
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nDisplay Pinball - perfil provisorio ESP32-3248S035R");
  display.init();
  display.setRotation(0);
  display.setBrightness(180);
  uint16_t calibration[8];
  if (preferences.begin("display", true)) {
    if (preferences.getBytesLength("touch-v1") == sizeof(calibration) &&
        preferences.getBytes("touch-v1", calibration, sizeof(calibration)) == sizeof(calibration)) {
      display.setTouchCalibrate(calibration);
    }
    preferences.end();
  }
  drawScreen();
  Serial.println("Envie c no monitor serial para calibrar o touch.");
}

void loop() {
  if (Serial.available() && Serial.read() == 'c') {
    calibrate();
  }
  int32_t x = 0, y = 0;
  const bool touched = display.getTouch(&x, &y);
  if (touched) {
    if (!wasTouched) {
      Serial.printf("Toque %lu: x=%ld y=%ld\n", ++touches, static_cast<long>(x), static_cast<long>(y));
      if (x >= 20 && x < 300 && y >= 420 && y < 460) {
        drawScreen();
      }
    }
    if (x >= 14 && x < 306 && y >= 144 && y < 396) {
      display.fillCircle(x, y, 3, TFT_YELLOW);
    }
  }
  wasTouched = touched;
  delay(15);
}
