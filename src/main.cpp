#include <Arduino.h>
#include <array>
#include <Wire.h>
#include <SPI.h>
#include "cs147_display.h"
#include <MQUnifiedsensor.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "comas_cloud.h"
#include "comas_pms5003.h"

#define getLoadingIcon() loading_sprites[(millis() / 1000) % 4]

namespace Pins {
    constexpr uint8_t buzzer = 33;
    constexpr uint8_t methane_sensor = 35;
    constexpr uint8_t co_sensor = 32;
    constexpr uint8_t led = 26;
    constexpr uint8_t rx_pin = 16;
    constexpr uint8_t tx_pin = 17;  //Unused, we dont send data to the sensor.
}

// Sensor calibration values.
constexpr float METHANE_RES_REF = 3.0f;
constexpr float CO_RES_REF = 9.0f;

constexpr float ADC_VOLTAGE = 3.3f;
constexpr float SENSOR_VCC = 5.0f;
constexpr uint8_t ADC_BITS = 12;

char loading_sprites[] = {'|', '/', '-', '\\'};

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

//Loop globals
unsigned long last_print_ms = 0;
unsigned long last_beep = 0;
unsigned long last_communication = 0;
bool beeping = false;
int remote_alert_node = 0;
unsigned long last_alert_poll_ms = 0;
unsigned long last_sample_ms = 0;
float methane_ppm;
float co_ppm;
struct PmsReading pms;
int alert_status;



float readMethanePPM() {
    float voltage = (analogReadMilliVolts(Pins::methane_sensor) / 1000.0f) * 2.0f;

    methane_sensor.externalADCUpdate(voltage);

    return methane_sensor.readSensor();
}

float readCOPPM() {
    float voltage = (analogReadMilliVolts(Pins::co_sensor) / 1000.0f) * 2.0f;

    co_sensor.externalADCUpdate(voltage);

    return co_sensor.readSensor();
}

int getAlertStatus(float methane_ppm, float co_ppm, struct PmsReading pms) {
    int result = 0;

    if (methane_ppm >= METHANE_PPM_THRESHOLD) result += 2;
    if (co_ppm >= CO_PPM_THRESHOLD) result += 1;
    if (pms.pm2_5 >= PM25_UGM3_THRESHOLD) result += 4;
    if (remote_alert_node) result += 8;
    
    return result;
}


void setup() {
    Serial.begin(115200);

    //I2C to display
    cs147InitDisplay();
    cs147DisplayLines(
        "Booting COMAS...",
        "",
        "Group: 18",
        "CS147"
    );
    delay(2000);

    // Buzzer & LED INIT
    pinMode(Pins::buzzer, OUTPUT);
    pinMode(Pins::led, OUTPUT);

    digitalWrite(Pins::buzzer, HIGH);
    digitalWrite(Pins::led, HIGH);
    delay(1000);
    digitalWrite(Pins::buzzer, LOW);
    digitalWrite(Pins::led, LOW);

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
    unsigned long wakeup_ms = millis() + SENSOR_WARMUP_MS;
    while (millis() < wakeup_ms) {
        cs147DisplayLines(
            "Booting COMAS...",
            "",
            "Sensors warming up.",
            "Done in " + String( (int)(wakeup_ms - millis())/1000) + "s."
        );
    }

    int wifi_attempts = 0;
    
    cs147DisplayLines(
                "Booting COMAS...",
                "",
                "Connecting to network\n" + String(COMAS_WIFI_SSID)
            );

    while (wifi_attempts < WIFI_MAX_ATTEMPTS) {
        wifi_attempts++;
        comasConnectWifi();

        if (!comasWifiOk()) {
            Serial.println("ERROR!");
            cs147DisplayLines(
                "Booting COMAS...",
                "",
                "Connecting to network\n" + String(COMAS_WIFI_SSID),
                "FAILURE!\nRetrying (" + String(wifi_attempts) + "/" + String(WIFI_MAX_ATTEMPTS) + ") " + getLoadingIcon()
            );
        } else {
            Serial.println("WiFi connected");
            cs147DisplayLines(
                "Booting COMAS...",
                "",
                "Connecting to network\n" + String(COMAS_WIFI_SSID),
                "Success!"
            );
            
            break;
        }
    }
    
    if (!comasWifiOk()) {
        cs147DisplayLines(
                "Booting COMAS...",
                "",
                "Unable to connect.",
                "Continuing locally."
            );
            delay(2000);
    }
}

void loop() {
    unsigned long now = millis();
    
    if (now - last_alert_poll_ms >= ALERT_POLL_INTERVAL_MS) {
        last_alert_poll_ms = now;

        remote_alert_node = comasPollRemoteAlert();
    }

    if (now - last_sample_ms >= SAMPLE_INTERVAL_MS) {
        last_sample_ms = now;

        methane_ppm = readMethanePPM();
        co_ppm = readCOPPM();
        pms = comasReadPms();
        alert_status = getAlertStatus(methane_ppm, co_ppm, pms); 
    }   

    // Print values
    if (now - last_print_ms >= PRINT_INTERVAL) {
        last_print_ms = now;

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

        String alert_text = "";
        if (alert_status & (1 << 0)) {
            alert_text = "CO DETECTED!";
        } else if (alert_status & (1 << 1)) {
            alert_text = "METHANE DETECTED!";
        } else if (alert_status & (1 << 2)) {
            alert_text = "High Particles!";
        } else if (alert_status & (1 << 3) && remote_alert_node != COMAS_NODE_ID) {
            alert_text = "UNSAFE LEVELS AT NODE " + String(remote_alert_node);
        }

        cs147DisplayLines(
            "Status: " + String(alert_text.length() > 0 ? "DANGER" : (comasWifiOk() ? "Operational" : "Local Only")),
            alert_text,
            "\nMethane: " + String(methane_ppm) + " ppm\nCO: " + String(co_ppm) + "ppm",
            "Particulates(ug/m^3)\nLarge: " + String(pms.pm10) + "\nFine: " + String(pms.pm2_5) + "\nUltrafine: " + String(pms.pm1_0)
        );
    }

    if (now - last_beep >= BEEP_INTERVAL) {
        last_beep = now;

        if (alert_status) {
            beeping = !beeping;
            if (beeping) {
                    digitalWrite(Pins::buzzer, HIGH);
                    digitalWrite(Pins::led, HIGH);
            } else {
                    digitalWrite(Pins::buzzer, LOW);
                    digitalWrite(Pins::led, LOW);
            }
        } else {
            digitalWrite(Pins::buzzer, LOW);
            digitalWrite(Pins::led, LOW);
        }
    }

    if (now - last_communication >= TELEMETRY_INTERVAL) {
        last_communication = now;

        comasPostTelemetry(
            co_ppm,
            methane_ppm,
            pms.pm2_5,
            pms.pm10,
            alert_status & ~(1 << 3)
        );
    }
}