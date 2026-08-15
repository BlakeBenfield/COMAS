#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "comas_config.h"

// wifi + cloud stuff (architecture 2: node -> wifi AP -> cloud server)
// handles both http (local flask server) and https (Render).
// the json we send is tiny and always the same shape so snprintf is fine,
// didn't feel like pulling in ArduinoJson just for this

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
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    return true;
  }
  Serial.println("Wi-Fi connection FAILED (will keep running locally).");
  return false;
}

inline bool comasWifiOk() {
  return WiFi.status() == WL_CONNECTED;
}

// picks http vs https based on the url
inline bool comasHttpBegin(HTTPClient& http, WiFiClientSecure& secureClient,
                           const String& url) {
  if (url.startsWith("https://")) {
    // not bothering with cert verification, it's a class project
    secureClient.setInsecure();
    return http.begin(secureClient, url);
  }
  return http.begin(url);
}

// upload one sample. alarmFlag is a bitmask: 1=CO, 2=methane, 4=particles (0 = all good)
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

// asks the server if any node has an alarm going. returns that node's id,
// or 0 if everything's fine (or the request failed, same difference here).
// server just sends back the id as plain text ("0"/"1"/"2")
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
