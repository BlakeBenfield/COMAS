#pragma once
#include <Arduino.h>
#include "comas_config.h"

// PMS5003 particle sensor, sits on UART2.
// it just spits out a 32 byte frame about once a second without being asked:
// 0x42 0x4D, 2 byte length, 13 uint16 data words, 2 byte checksum.
// we only pull out PM1.0/PM2.5/PM10 (the "atmospheric" set, words 4-6 --
// the datasheet has two sets and the other one is for factory calibration)

struct PmsReading {
  uint16_t pm1_0 = 0;
  uint16_t pm2_5 = 0;
  uint16_t pm10  = 0;
  bool valid = false;
};

inline void comasInitPms() {
  Serial1.begin(9600, SERIAL_8N1, Pins::pms_rx, Pins::pms_tx);  // 9600 8N1 per datasheet
}

// drain whatever's in the uart buffer and return the newest good frame
inline PmsReading comasReadPms() {
  PmsReading result;
  uint8_t frame[32];

  while (Serial1.available() >= 32) {
    if (Serial1.peek() != 0x42) {
      Serial1.read();  // not at a frame start, throw bytes away until we are
      continue;
    }
    Serial1.readBytes(frame, 32);
    if (frame[1] != 0x4D) {
      continue;
    }

    // checksum = sum of bytes 0..29, stored big-endian in the last 2 bytes
    uint16_t sum = 0;
    for (int i = 0; i < 30; i++) {
      sum += frame[i];
    }
    const uint16_t expected = (uint16_t)(frame[30] << 8) | frame[31];
    if (sum != expected) {
      continue;
    }

    result.pm1_0 = (uint16_t)(frame[10] << 8) | frame[11];
    result.pm2_5 = (uint16_t)(frame[12] << 8) | frame[13];
    result.pm10  = (uint16_t)(frame[14] << 8) | frame[15];
    result.valid = true;
    // don't break here -- if there's a backlog we want the latest frame not the oldest
  }
  return result;
}