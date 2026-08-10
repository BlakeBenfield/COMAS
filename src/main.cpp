// Header files. Do NOT comment these!
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include "cs147_display.h"
#include "cs147_common.h"
#include "comas_config.h"
#include "comas_pms5003.h"
#include "comas_cloud.h"
 

constexpr uint8_t CS147_OLED_ADDRESS = 0x3C;

namespace Pins {
  constexpr uint8_t buzzer = 33;
  constexpr uint8_t methane_sensor = 2;
  constexpr uint8_t co_sensor = 4;
}

void setup() {
/* 
  INIT
    I2C
    UART
    BUZZER PIN
    DISPLAY
    ADC2 CH2 CH 0
    

  
  BEEP
  Show init text
*/
  //I2C to display
  cs147InitDisplay();   
  cs147DisplayLines(
    "Booting COMAS...",
    "",
    "Group: 18",
    "CS147"
  );
  
  // Buzzer INIT
  pinMode(Pins::buzzer, OUTPUT);
  digitalWrite(Pins::buzzer, HIGH);
  delay(1000);
  digitalWrite(Pins::buzzer, LOW);

  Serial1.begin(9600);  //UART to particle sensor

  //wait 30s for part. sensor to wakeup
  Serial1.flush();  // Remove garbage data
  
}

void loop() {
/*
  every 15 seconds
    Check methane
    Check CO
    Check particle 
    Update display

    Check thresholds
    IF too high
      Beep Buzzer
      flash LED
      append error data to cloud

    send data to cloud
*/
}

