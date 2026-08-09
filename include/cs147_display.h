#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

constexpr int CS147_SCREEN_WIDTH = 128;
constexpr int CS147_SCREEN_HEIGHT = 64;

// The most common address for this four-pin I2C OLED.
// Change to 0x3D only if the I2C scanner reports 0x3D.
constexpr uint8_t CS147_OLED_ADDRESS = 0x3C;

inline Adafruit_SSD1306& cs147Display() {
  // -1 means the OLED does not use a separate ESP32 reset pin.
  static Adafruit_SSD1306 display(
      CS147_SCREEN_WIDTH,
      CS147_SCREEN_HEIGHT,
      &Wire,
      -1);

  return display;
}

inline bool cs147InitDisplay() {
  auto& display = cs147Display();
 if (!display.begin(
          SSD1306_SWITCHCAPVCC,
          CS147_OLED_ADDRESS)) {
    Serial.println(
        "OLED initialization failed. "
        "Check SDA, SCL, power, and I2C address.");
    return false;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("CS147 IoT Kit");
  display.display();

  return true;
}

inline void cs147DisplayLines(
    const String& line1,
    const String& line2 = "",
    const String& line3 = "",
    const String& line4 = "") {

  auto& display = cs147Display();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  display.println(line1);

  if (line2.length()) {
    display.println(line2);
  }
  if (line3.length()) {
    display.println(line3);
  }
  if (line4.length()) {
    display.println(line4);
  }

  display.display();
}
