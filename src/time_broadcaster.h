#pragma once

#include <WiFi.h>
#include <WiFiUdp.h>

static constexpr uint16_t DISCOVERY_PORT = 5354;
static constexpr uint32_t DISCOVERY_INTERVAL = 16000;

class TimeBroadcaster {
  WiFiUDP _udp;
  uint32_t _last_broadcast = 0;
  bool _running = false;

public:
  bool begin() {
    _running = _udp.begin(DISCOVERY_PORT);
    return _running;
  }

  void loop(uint32_t unix_time, bool clock_synchronized) {
    if (!_running) return;
    if (!clock_synchronized) return;

    uint32_t now = millis();
    if (now - _last_broadcast < DISCOVERY_INTERVAL) return;
    _last_broadcast = now;

    char json[384];
    snprintf(json, sizeof(json),
      "{\"type\":\"TIME_ANNOUNCE\","
      "\"message_id\":\"gps-timeserver-%lu\","
      "\"timestamp\":%llu000000000,"
      "\"clock_info\":{\"stratum\":1,\"precision\":-20,"
      "\"root_delay\":0.0,\"root_dispersion\":0.0001,"
      "\"reference_id\":\"GPS\",\"reference_time\":%llu000000000},"
      "\"leap_indicator\":0,"
      "\"source_id\":\"gps-timeserver\"}",
      now, (unsigned long long)unix_time, (unsigned long long)unix_time);

    _udp.beginPacket(IPADDR_BROADCAST, DISCOVERY_PORT);
    _udp.write((const uint8_t*)json, strlen(json));
    _udp.endPacket();
  }
};
