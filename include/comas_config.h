#pragma once
#include <Arduino.h>

// COMAS config - team 18, CS147
// pretty much everything you'd want to tweak is in this file

// change this to 2 before flashing the second board!
constexpr int COMAS_NODE_ID = 2;

// wifi creds
constexpr const char* COMAS_WIFI_SSID = "YOUR_WIFI_SSID";
constexpr const char* COMAS_WIFI_PASS = "YOUR_WIFI_PASSWORD";
constexpr const int WIFI_MAX_ATTEMPTS = 3;

// server base url, no trailing slash. local IP while testing,
// swap in the EC2/azure ip once it's deployed
constexpr const char* COMAS_SERVER = "https://comas-8tcc.onrender.com/";

// timing
constexpr uint32_t SAMPLE_INTERVAL_MS      = 2000;  // sample + upload every 15s
constexpr uint32_t ALERT_POLL_INTERVAL_MS  = 10000;  // check for alerts from the other node
constexpr uint32_t SENSOR_WARMUP_MS        = 2000;  // MQ heaters + PMS need time to wake up
constexpr uint32_t BEEP_INTERVAL           = 1000;
constexpr uint32_t TELEMETRY_INTERVAL      = 20000;
constexpr uint32_t PRINT_INTERVAL          = 3000;

// alarm thresholds
constexpr int CO_PPM_THRESHOLD      = 70;   //ppm, standard CO alarm value
constexpr int METHANE_PPM_THRESHOLD = 750; //ppm, 10% of methanes explosive limit
constexpr int PM25_UGM3_THRESHOLD   = 150;  // ug/m3, this is already "unhealthy" on the AQI scale

