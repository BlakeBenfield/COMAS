#pragma once
#include <Arduino.h>
#include "comas_config.h"

struct PmsReading {
    uint16_t pm1_0 = 0;
    uint16_t pm2_5 = 0;
    uint16_t pm10  = 0;
};

inline void comasInitPms(int8_t RX_PIN, int8_t TX_PIN) {
    Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
}

inline PmsReading comasReadPms() {
    PmsReading result;
    uint8_t data[32];

    while (Serial2.available() >= 32) {

        // Search for first header byte
        if (Serial2.peek() != 0x42) {
            Serial2.read();
            continue;
        }

        Serial2.readBytes(data, 32);

        // Validate header
        if (data[0] != 0x42 || data[1] != 0x4D) {
            continue;
        }

        // Validate checksum
        uint16_t sum = 0;

        for (int i = 0; i < 30; i++) {
            sum += data[i];
        }

        uint16_t expected =
            ((uint16_t)data[30] << 8) |
            data[31];

        if (sum != expected) {
            continue;
        }

        // PM values
        result.pm1_0 =
            ((uint16_t)data[10] << 8) |
            data[11];

        result.pm2_5 =
            ((uint16_t)data[12] << 8) |
            data[13];

        result.pm10 =
            ((uint16_t)data[14] << 8) |
            data[15];
    }

    return result;
}