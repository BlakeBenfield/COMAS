// Header files. Do NOT comment these!
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include "cs147_display.h"
#include <MQUnifiedsensor.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "comas_cloud.h"
#include "comas_pms5003.h"

namespace Pins {
    constexpr uint8_t buzzer = 33;
    constexpr uint8_t methane_sensor = 35;
    constexpr uint8_t co_sensor = 32;
    constexpr uint8_t led = 26;
    constexpr uint8_t rx_pin = 16;
    constexpr uint8_t tx_pin = 17;  //Unused, we dont send data to the sensor.
}

// Sensor calibration values.
// Adjust these manually after the sensors have warmed up.
constexpr float METHANE_RES_REF = 3.0f;
constexpr float CO_RES_REF = 9.0f;

constexpr float ADC_VOLTAGE = 3.3f;
constexpr float SENSOR_VCC = 5.0f;
constexpr uint8_t ADC_BITS = 12;

// Sensors
MQUnifiedsensor methane_sensor(
    "ESP32",
    ADC_VOLTAGE,
    ADC_BITS,
    Pins::methane_sensor,
    "MQ-2"
);

MQUnifiedsensor co_sensor(
    "ESP32",
    ADC_VOLTAGE,
    ADC_BITS,
    Pins::co_sensor,
    "MQ-7"
);

float readMethanePPM() {
    float voltage =
        (analogReadMilliVolts(Pins::methane_sensor) / 1000.0f) * 2.0f;

    methane_sensor.externalADCUpdate(voltage);

    return methane_sensor.readSensor();
}

float readCOPPM() {
    float voltage =
        (analogReadMilliVolts(Pins::co_sensor) / 1000.0f) * 2.0f;

    co_sensor.externalADCUpdate(voltage);

    return co_sensor.readSensor();
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

    Serial.begin(115200);

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

    // ADC INIT
    analogReadResolution(12);

    // MQ-2 INIT
    methane_sensor.init();
    methane_sensor.setVCC(SENSOR_VCC);
    methane_sensor.setRL(1);
    methane_sensor.setR0(METHANE_RES_REF);
    methane_sensor.setRegressionMethod(1);
    methane_sensor.setA(4309);
    methane_sensor.setB(-2.625);

    // MQ-7 INIT
    co_sensor.init();
    co_sensor.setVCC(SENSOR_VCC);
    co_sensor.setRL(10);
    co_sensor.setR0(CO_RES_REF);
    co_sensor.setRegressionMethod(1);
    co_sensor.setA(99.042);
    co_sensor.setB(-1.518);

    comasInitPms(Pins::rx_pin, Pins::tx_pin);

    //wait for particle sensor to wakeup
    delay(SENSOR_WARMUP_MS);

    Serial1.flush();  // Remove garbage data

    comasConnectWifi();

    if (!comasWifiOk())
        Serial.println("ERROR!");
    else
        Serial.println("WiFi connected");
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

    float methane_ppm = readMethanePPM();
    float co_ppm = readCOPPM();
    struct PmsReading pms = comasReadPms();

    Serial.print("Methane: ");
    Serial.print(methane_ppm);
    Serial.print(" ppm | CO: ");
    Serial.print(co_ppm);
    Serial.println(" ppm");

    Serial.print(" PM1.0=");
    Serial.print(pms.pm1_0);

    Serial.print(" PM2.5=");
    Serial.print(pms.pm2_5);

    Serial.print(" PM10=");
    Serial.println(pms.pm10);

    Serial.println(comasPollRemoteAlert());

    //TODO alarm, display
    comasPostTelemetry(
        co_ppm,
        methane_ppm,
        pms.pm2_5,
        pms.pm1_0,
        0
    );

    delay(SAMPLE_INTERVAL_MS);
}