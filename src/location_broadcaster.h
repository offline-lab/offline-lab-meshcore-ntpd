#pragma once

#include <WiFi.h>
#include <WiFiUdp.h>
#include <time.h>

static constexpr uint16_t LOCATION_BROADCAST_PORT = 5354;
static constexpr uint32_t LOCATION_BROADCAST_INTERVAL = 60000;

class LocationBroadcaster {
  WiFiUDP _udp;
  uint32_t _last_broadcast = 0;
  bool _running = false;

public:
  bool begin() {
    _running = _udp.begin(LOCATION_BROADCAST_PORT + 1);
    return _running;
  }

  void loop(double lat, double lon, double alt, long satellites, bool fix) {
    if (!_running || !fix) return;

    uint32_t now = millis();
    if (now - _last_broadcast < LOCATION_BROADCAST_INTERVAL) return;
    _last_broadcast = now;

    char json[256];
    snprintf(json, sizeof(json),
      "{\"type\":\"LOCATION_ANNOUNCE\","
      "\"message_id\":\"gps-timeserver-%lu\","
      "\"timestamp\":%lu,"
      "\"source_id\":\"gps-timeserver\","
      "\"location\":{\"latitude\":%.6f,\"longitude\":%.6f,"
      "\"altitude\":%.1f,\"fix\":true,\"satellites\":%ld}}",
      (unsigned long)now, (unsigned long)time(nullptr),
      lat, lon, alt, satellites);

    _udp.beginPacket(IPADDR_BROADCAST, LOCATION_BROADCAST_PORT);
    _udp.write((const uint8_t*)json, strlen(json));
    _udp.endPacket();
  }
};
