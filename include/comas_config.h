#pragma once
#include <Arduino.h>

// COMAS config - team 18, CS147
// pretty much everything you'd want to tweak is in this file

// change this to 2 before flashing the second board!
constexpr int COMAS_NODE_ID = 1;

// wifi creds (arch 2: node -> AP -> cloud)
constexpr const char* COMAS_WIFI_SSID = "YOUR_WIFI_SSID";
constexpr const char* COMAS_WIFI_PASS = "YOUR_WIFI_PASSWORD";

// server base url, no trailing slash. local IP while testing,
// swap in the EC2/azure ip once it's deployed
constexpr const char* COMAS_SERVER = "http://192.168.1.50:5000";

namespace Pins {
  constexpr uint8_t buzzer = 33;

  // NOTE: we originally had CO on GPIO 4 and methane on GPIO 2, but those are
  // ADC2 pins and ADC2 just doesn't work while wifi is on (took a while to
  // figure that one out). had to rewire both to ADC1 pins:
  constexpr uint8_t co_sensor      = 34;  // MQ-7 AO (input-only pin)
  constexpr uint8_t methane_sensor = 35;  // MQ-2 AO (also input-only)

  constexpr uint8_t led = 26;

  // PMS5003 goes on UART2, can't use RX0/TX0 since that's the usb serial
  constexpr uint8_t pms_rx = 16;  // sensor TX (P5) -> here
  constexpr uint8_t pms_tx = 17;  // sensor RX (P4), only needed if we send it commands
}

// timing
constexpr uint32_t SAMPLE_INTERVAL_MS      = 15000;  // sample + upload every 15s
constexpr uint32_t ALERT_POLL_INTERVAL_MS  = 10000;  // check for alerts from the other node
constexpr uint32_t SENSOR_WARMUP_MS        = 30000;  // MQ heaters + PMS need time to wake up

// alarm thresholds
// the MQ values are raw ADC (0-4095), not calibrated to ppm or anything.
// watch the serial output in clean air and set these a good bit above
// whatever baseline you see. these worked ok for us as starting points
constexpr int CO_RAW_THRESHOLD      = 2000;
constexpr int METHANE_RAW_THRESHOLD = 2000;
constexpr int PM25_UGM3_THRESHOLD   = 150;   // ug/m3, this is already "unhealthy" on the AQI scale