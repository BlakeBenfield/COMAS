#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "comas_config.h"

// -----------------------------------------------------------------------------
// Wi-Fi + cloud client (Architecture 2: node -> Wi-Fi AP -> cloud server).
// Works with both http:// (local server) and https:// (Render/hosted server).
// JSON is small and fixed-shape, so we build it with snprintf instead of
// pulling in a JSON library.
// -----------------------------------------------------------------------------

inline bool comasConnectWifi(uint32_t timeoutMs = 15000) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(COMAS_WIFI_SSID, COMAS_WIFI_PASS);
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi connected, IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println("Wi-Fi connection FAILED (will keep running locally).");
  return false;
}

inline bool comasWifiOk() {
  return WiFi.status() == WL_CONNECTED;
}

// Begins an HTTP or HTTPS request depending on the server URL scheme.
// Returns false if begin() failed.
inline bool comasHttpBegin(HTTPClient& http, WiFiClientSecure& secureClient,
                           const String& url) {
  if (url.startsWith("https://")) {
    // Course project: skip certificate verification for simplicity.
    secureClient.setInsecure();
    return http.begin(secureClient, url);
  }
  return http.begin(url);
}

// POST one telemetry sample. alarmFlag: 0 = normal, else bitmask
// (1 = CO, 2 = methane, 4 = particles).
inline bool comasPostTelemetry(int coRaw, int methaneRaw, int pm25,
                               int pm10, int alarmFlag) {
  if (!comasWifiOk()) return false;

  char body[192];
  snprintf(body, sizeof(body),
           "{\"node_id\":%d,\"co_raw\":%d,\"methane_raw\":%d,"
           "\"pm25\":%d,\"pm10\":%d,\"alarm\":%d}",
           COMAS_NODE_ID, coRaw, methaneRaw, pm25, pm10, alarmFlag);

  WiFiClientSecure secureClient;
  HTTPClient http;
  if (!comasHttpBegin(http, secureClient,
                      String(COMAS_SERVER) + "/api/telemetry")) {
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(body);
  http.end();

  if (code != 200) {
    Serial.print("Telemetry POST failed, HTTP ");
    Serial.println(code);
    return false;
  }
  return true;
}

// Ask the server whether ANY node currently has an active alarm.
// Returns the alarming node id, or 0 if all clear / request failed.
// Response body is just the node id as plain text ("0", "1", "2").
inline int comasPollRemoteAlert() {
  if (!comasWifiOk()) return 0;

  WiFiClientSecure secureClient;
  HTTPClient http;
  if (!comasHttpBegin(http, secureClient,
                      String(COMAS_SERVER) + "/api/active_alert")) {
    return 0;
  }
  const int code = http.GET();
  int alertNode = 0;
  if (code == 200) {
    alertNode = http.getString().toInt();
  }
  http.end();
  return alertNode;
}
