#pragma once

#include <WiFi.h>

#ifndef WIFI_SSID
#error "WIFI_SSID must be defined at build time"
#endif
#ifndef WIFI_PASS
#error "WIFI_PASS must be defined at build time"
#endif
static_assert(sizeof(WIFI_SSID) > 1, "WIFI_SSID is empty — set the WIFI_SSID env var");
static_assert(sizeof(WIFI_PASS) > 1, "WIFI_PASS is empty — set the WIFI_PASS env var");

static constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
static constexpr unsigned long WIFI_RECONNECT_INTERVAL_MS = 10000;

class SimpleWiFiManager {
  bool _connected = false;
  unsigned long _last_attempt = 0;

public:
  bool begin() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
      delay(250);
    }

    _connected = (WiFi.status() == WL_CONNECTED);
    return _connected;
  }

  void loop() {
    if (WiFi.status() == WL_CONNECTED) {
      _connected = true;
      return;
    }

    _connected = false;
    unsigned long now = millis();
    if (now - _last_attempt < WIFI_RECONNECT_INTERVAL_MS) return;

    _last_attempt = now;
    WiFi.reconnect();
  }

  bool isConnected() const { return _connected; }
};
