// Header files. Do NOT comment these!
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include "cs147_display.h"
#include <MQUnifiedsensor.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

constexpr char WIFI_SSID[] = "iPhone";
constexpr char WIFI_PASSWORD[] = "PassworD";

constexpr char CLOUD_URL[] = "https://192.0.0.2:5000";

namespace Pins {
constexpr uint8_t buzzer = 33;
constexpr uint8_t methane_sensor = 35; // MQ-2
constexpr uint8_t co_sensor = 32;      // MQ-7
}

// Sensor calibration values.
constexpr float METHANE_RES_REF = 10.0f;
constexpr float CO_RES_REF = 10.0f;

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

bool uploadData(float methane_ppm, float co_ppm) {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    WiFiClientSecure client;

    client.setInsecure();

    HTTPClient http;

    if (!http.begin(client, CLOUD_URL)) {
        return false;
    }

    http.addHeader("Content-Type", "application/json");

    String data =
        "{\"methane_ppm\":" + String(methane_ppm, 2) +
        ",\"co_ppm\":" + String(co_ppm, 2) + "}";

    int response = http.POST(data);

    Serial.print("Cloud response: ");
    Serial.println(response);

    http.end();

    return response >= 200 && response < 300;
}

float readPPM(uint8_t pin) {
    MQUnifiedsensor *sensor;

    if (pin == Pins::methane_sensor) {
        sensor = &methane_sensor;
    } else {
        sensor = &co_sensor;
    }

    float voltage =
        (analogReadMilliVolts(pin) / 1000.0f) * 2.0f;

    sensor->externalADCUpdate(voltage);

    return sensor->readSensor();
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

    // MQ-7 INIT
    co_sensor.init();
    co_sensor.setVCC(SENSOR_VCC);
    co_sensor.setRL(1);
    co_sensor.setR0(CO_RES_REF);
    co_sensor.setRegressionMethod(1);
    co_sensor.setA(99.042);
    co_sensor.setB(-1.518);


    Serial1.begin(9600);  //UART to particle sensor

    //wait 30s for part. sensor to wakeup
    delay(30 * 1000);

    Serial1.flush();  // Remove garbage data

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }

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

    float methane_ppm = readPPM(Pins::methane_sensor);
    float co_ppm = readPPM(Pins::co_sensor);

    Serial.print("Methane: ");
    Serial.print(methane_ppm);
    Serial.print(" ppm | CO: ");
    Serial.print(co_ppm);
    Serial.println(" ppm");

    delay(15 * 1000);
}