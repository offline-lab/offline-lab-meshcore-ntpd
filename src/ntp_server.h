#pragma once

#include <WiFi.h>
#include <WiFiUdp.h>

enum class NtpStratum : uint8_t {
    PRIMARY   = 1,
    SECONDARY = 2,
    UNSYNC    = 16,
};

static inline NtpStratum currentStratum(bool gps_has_fix, bool clock_synchronized) {
    if (gps_has_fix)     return NtpStratum::PRIMARY;
    if (clock_synchronized) return NtpStratum::SECONDARY;
    return NtpStratum::UNSYNC;
}

static constexpr uint16_t NTP_PORT = 123;
static constexpr int NTP_PACKET_SIZE = 48;
static constexpr uint32_t NTP_EPOCH_OFFSET = 2208988800UL;

class NTPServer {
  WiFiUDP _udp;
  uint32_t _query_count = 0;
  bool _running = false;

  static void writeNtpTimestamp(uint8_t* pkt, int offset, uint32_t ntp_time) {
    pkt[offset]     = (ntp_time >> 24) & 0xFF;
    pkt[offset + 1] = (ntp_time >> 16) & 0xFF;
    pkt[offset + 2] = (ntp_time >> 8) & 0xFF;
    pkt[offset + 3] = ntp_time & 0xFF;
    memset(&pkt[offset + 4], 0, 4);
  }

public:
  bool begin() {
    _running = _udp.begin(NTP_PORT);
    return _running;
  }

  void loop(uint32_t unix_time, bool clock_synchronized, bool gps_has_fix) {
    if (!_running) return;

    int size = _udp.parsePacket();
    if (size < NTP_PACKET_SIZE) return;

    uint8_t pkt[NTP_PACKET_SIZE];
    _udp.read(pkt, NTP_PACKET_SIZE);

    memcpy(&pkt[24], &pkt[40], 8);

    NtpStratum stratum = currentStratum(gps_has_fix, clock_synchronized);

    pkt[0] = 0x24;
    pkt[1] = static_cast<uint8_t>(stratum);
    pkt[3] = 0xFA;
    memset(&pkt[4], 0, 8);

    if (clock_synchronized) {
      pkt[12] = 'G'; pkt[13] = 'P'; pkt[14] = 'S'; pkt[15] = 0;
    } else {
      memset(&pkt[12], 0, 4);
    }

    uint32_t ntp_time = unix_time + NTP_EPOCH_OFFSET;
    writeNtpTimestamp(pkt, 16, ntp_time);
    writeNtpTimestamp(pkt, 32, ntp_time);
    writeNtpTimestamp(pkt, 40, ntp_time);

    _udp.beginPacket(_udp.remoteIP(), _udp.remotePort());
    _udp.write(pkt, NTP_PACKET_SIZE);
    _udp.endPacket();

    _query_count++;
  }

  uint32_t getQueryCount() const { return _query_count; }
};
