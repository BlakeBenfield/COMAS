#pragma once
#include <Arduino.h>

// COMAS config - team 18, CS147
// pretty much everything you'd want to tweak is in this file

// change this to 2 before flashing the second board!
constexpr int COMAS_NODE_ID = 1;

// wifi creds
constexpr const char* COMAS_WIFI_SSID = "NAME";
constexpr const char* COMAS_WIFI_PASS = "PASS";

// server base url, no trailing slash. local IP while testing,
// swap in the EC2/azure ip once it's deployed
constexpr const char* COMAS_SERVER = "http://172.20.10.7:5000";

// timing
constexpr uint32_t SAMPLE_INTERVAL_MS      = 15000;  // sample + upload every 15s
constexpr uint32_t ALERT_POLL_INTERVAL_MS  = 10000;  // check for alerts from the other node
constexpr uint32_t SENSOR_WARMUP_MS        = 30000;  // MQ heaters + PMS need time to wake up

// alarm thresholds
constexpr int CO_RAW_THRESHOLD      = 70;   //ppm, standard CO alarm value
constexpr int METHANE_RAW_THRESHOLD = 5000; //ppm, 10% of methanes explosive limit
constexpr int PM25_UGM3_THRESHOLD   = 150;  // ug/m3, this is already "unhealthy" on the AQI scale