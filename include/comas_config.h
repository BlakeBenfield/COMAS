#pragma once
#include <Arduino.h>

// -----------------------------------------------------------------------------
// COMAS configuration — Team 18, CS147
// Everything you should ever need to edit lives in this file.
// -----------------------------------------------------------------------------

// Set to 1 on the first board, 2 on the second board before flashing.
constexpr int COMAS_NODE_ID = 1;

// --- Wi-Fi (Architecture 2: sensor node -> Wi-Fi AP -> cloud) ---
constexpr const char* COMAS_WIFI_SSID = "YOUR_WIFI_SSID";
constexpr const char* COMAS_WIFI_PASS = "YOUR_WIFI_PASSWORD";

// --- Cloud server base URL (no trailing slash) ---
// e.g. "http://192.168.1.50:5000" while testing locally,
// or "http://<your-ec2-or-azure-vm-ip>:5000" once deployed.
constexpr const char* COMAS_SERVER = "http://192.168.1.50:5000";

namespace Pins {
  constexpr uint8_t buzzer = 33;

  // IMPORTANT: original wiring used GPIO 4 (CO) and GPIO 2 (methane).
  // Those are ADC2 pins and ADC2 does NOT work while Wi-Fi is active.
  // Re-wire the two analog outputs to the ADC1 pins below.
  constexpr uint8_t co_sensor      = 34;  // MQ-7  AO -> GPIO 34 (ADC1_CH6, input-only)
  constexpr uint8_t methane_sensor = 35;  // MQ-2  AO -> GPIO 35 (ADC1_CH7, input-only)

  constexpr uint8_t led = 26;

  // PMS5003 on UART2 (RX0/TX0 clashes with USB serial / flashing).
  constexpr uint8_t pms_rx = 16;  // PMS5003 TX (P5) -> ESP32 GPIO 16
  constexpr uint8_t pms_tx = 17;  // PMS5003 RX (P4) -> ESP32 GPIO 17 (optional, only for commands)
}

// --- Timing ---
constexpr uint32_t SAMPLE_INTERVAL_MS      = 15000;  // read sensors + upload every 15 s
constexpr uint32_t ALERT_POLL_INTERVAL_MS  = 10000;  // ask cloud about other nodes every 10 s
constexpr uint32_t SENSOR_WARMUP_MS        = 30000;  // MQ heater + PMS5003 wake-up time

// --- Alarm thresholds ---
// MQ readings are raw 12-bit ADC values (0-4095), uncalibrated.
// Calibrate by watching Serial output in clean air, then pick thresholds
// comfortably above the clean-air baseline. Starting points below.
constexpr int CO_RAW_THRESHOLD      = 2000;
constexpr int METHANE_RAW_THRESHOLD = 2000;
constexpr int PM25_UGM3_THRESHOLD   = 150;   // ug/m3, "unhealthy" range