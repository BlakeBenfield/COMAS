#pragma once
#include <Arduino.h>
#include "comas_config.h"

// -----------------------------------------------------------------------------
// PMS5003 particle sensor over UART2.
// The sensor pushes a 32-byte frame roughly once per second:
//   0x42 0x4D | frame length (2B) | 13 x uint16 data words | checksum (2B)
// We read PM1.0 / PM2.5 / PM10 (atmospheric-environment values, words 4-6).
// -----------------------------------------------------------------------------

struct PmsReading {
  uint16_t pm1_0 = 0;
  uint16_t pm2_5 = 0;
  uint16_t pm10  = 0;
  bool valid = false;
};

inline void comasInitPms() {
  // UART2, 9600 8N1, remapped pins.
  Serial1.begin(9600, SERIAL_8N1, Pins::pms_rx, Pins::pms_tx);
}

// Drains the UART buffer and returns the most recent complete valid frame.
inline PmsReading comasReadPms() {
  PmsReading result;
  uint8_t frame[32];

  while (Serial1.available() >= 32) {
    if (Serial1.peek() != 0x42) {
      Serial1.read();  // resync: discard until start byte
      continue;
    }
    Serial1.readBytes(frame, 32);
    if (frame[1] != 0x4D) {
      continue;
    }

    // Checksum: sum of bytes 0..29 must equal the last two bytes.
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
    // Keep looping: if more frames are buffered, prefer the newest one.
  }
  return result;
}