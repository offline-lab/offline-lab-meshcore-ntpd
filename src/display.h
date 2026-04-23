#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <helpers/ui/DisplayDriver.h>
#include <helpers/CommonCLI.h>
#include <target.h>
#include "ntp_server.h"
#include "wifi_manager.h"

class UITask {
  static constexpr unsigned long AUTO_OFF_MILLIS = 60000;
  static constexpr unsigned long BOOT_SCREEN_MILLIS = 4000;
  static constexpr unsigned long REFRESH_INTERVAL = 2000;
  static constexpr int LINE_H = 10;
  static constexpr int NUM_PAGES = 3;
  static constexpr int MAX_LINES = 6;
  static constexpr int LINE_BUF = 22;

  DisplayDriver* _display;
  unsigned long _next_read = 0;
  unsigned long _next_refresh = 0;
  unsigned long _auto_off = 0;
  int _prev_btn = HIGH;
  int _page = 0;
  int _prev_page = -1;
  NodePrefs* _node_prefs = nullptr;
  NTPServer* _ntp = nullptr;
  SimpleWiFiManager* _wifi = nullptr;
  bool _clock_synchronized = false;
  double _last_lat = 0.0;
  double _last_lon = 0.0;
  double _last_alt = 0.0;
  bool _has_last_position = false;
  char _version_info[32] = {};
  char _prev_lines[MAX_LINES][LINE_BUF] = {};

public:
  explicit UITask(DisplayDriver& display) : _display(&display) {}

  void setClockSynchronized(bool v) { _clock_synchronized = v; }

  void begin(NodePrefs* node_prefs, const char* build_date, const char* firmware_version,
             NTPServer* ntp, SimpleWiFiManager* wifi) {
    _node_prefs = node_prefs;
    _ntp = ntp;
    _wifi = wifi;
    _auto_off = millis() + AUTO_OFF_MILLIS;
    _display->turnOn();

    char version[16];
    strncpy(version, firmware_version, sizeof(version) - 1);
    version[sizeof(version) - 1] = 0;
    char* dash = strchr(version, '-');
    if (dash) *dash = 0;
    snprintf(_version_info, sizeof(_version_info), "%s (%s)", version, build_date);
  }

  void loop() {
    unsigned long now = millis();

    if (now >= _next_read) {
      int btn = digitalRead(PIN_USER_BTN);
      if (btn != _prev_btn && btn == LOW) {
        if (!_display->isOn()) _display->turnOn();
        _page = (_page + 1) % NUM_PAGES;
        _auto_off = now + AUTO_OFF_MILLIS;
        _next_refresh = 0;
      }
      _prev_btn = btn;
      _next_read = now + 200;
    }

    if (!_display->isOn()) return;

    if (now >= _next_refresh) {
      render();
      _next_refresh = now + REFRESH_INTERVAL;
    }

    if (now > _auto_off) {
      _display->turnOff();
    }
  }

private:
  void clearLine(int idx) {
    _display->setColor(DisplayDriver::DARK);
    _display->fillRect(0, idx * LINE_H, 128, LINE_H);
  }

  void drawLine(int idx, DisplayDriver::Color color, const char* text) {
    if (idx < 0 || idx >= MAX_LINES) return;
    if (_page == _prev_page && strncmp(_prev_lines[idx], text, LINE_BUF) == 0) return;
    strncpy(_prev_lines[idx], text, LINE_BUF - 1);
    _prev_lines[idx][LINE_BUF - 1] = 0;
    clearLine(idx);
    if (text[0]) {
      _display->setColor(color);
      _display->setCursor(0, idx * LINE_H);
      _display->print(text);
    }
  }

  void render() {
    _display->setTextSize(1);

    if (millis() < BOOT_SCREEN_MILLIS) {
      _display->startFrame();
      _display->setColor(DisplayDriver::LIGHT);
      _display->setCursor(0, 0);
      _display->print("GPS TimeServer");
      _display->setCursor(0, LINE_H);
      _display->print(_version_info);
      _display->endFrame();
      _prev_page = 0;
      return;
    }

    if (_page != _prev_page) {
      _display->startFrame();
      _display->endFrame();
      memset(_prev_lines, 0, sizeof(_prev_lines));
      _prev_page = _page;
    }

    char tmp[LINE_BUF];
    switch (_page) {
      case 0: renderStatus(tmp); break;
      case 1: renderMesh(tmp); break;
      case 2: renderGPS(tmp); break;
    }
  }

  void renderStatus(char* tmp) {
    bool wifi_up = _wifi && _wifi->isConnected();
    if (wifi_up) {
      IPAddress ip = WiFi.localIP();
      snprintf(tmp, LINE_BUF, "WiFi %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    } else {
      snprintf(tmp, LINE_BUF, "WiFi ---");
    }
    drawLine(0, !wifi_up ? DisplayDriver::RED : DisplayDriver::LIGHT, tmp);

    LocationProvider* loc = sensors.getLocationProvider();
    bool fix = loc && loc->isValid();
    snprintf(tmp, LINE_BUF, "GPS:%s Sat:%ld", fix ? "FIX" : "---",
             loc ? loc->satellitesCount() : 0L);
    drawLine(1, !fix ? DisplayDriver::RED : DisplayDriver::LIGHT, tmp);

    time_t now;
    time(&now);
    struct tm* t = gmtime(&now);
    snprintf(tmp, LINE_BUF, "%04d-%02d-%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
    drawLine(2, DisplayDriver::LIGHT, tmp);

    snprintf(tmp, LINE_BUF, "%02d:%02d:%02d UTC", t->tm_hour, t->tm_min, t->tm_sec);
    drawLine(3, DisplayDriver::LIGHT, tmp);

    snprintf(tmp, LINE_BUF, "NTP:%lu",
             _ntp ? (unsigned long)_ntp->getQueryCount() : 0UL);
    drawLine(4, DisplayDriver::LIGHT, tmp);

    snprintf(tmp, LINE_BUF, "F:%06.3f SF%d", _node_prefs->freq, _node_prefs->sf);
    drawLine(5, DisplayDriver::LIGHT, tmp);
  }

  void renderMesh(char* tmp) {
    drawLine(0, DisplayDriver::LIGHT, "-- Mesh --");

    snprintf(tmp, LINE_BUF, "%.15s", _node_prefs->node_name);
    drawLine(1, DisplayDriver::LIGHT, tmp);

    snprintf(tmp, LINE_BUF, "FWD:%s", _node_prefs->disable_fwd ? "OFF" : "ON");
    drawLine(2, _node_prefs->disable_fwd ? DisplayDriver::RED : DisplayDriver::LIGHT, tmp);

    snprintf(tmp, LINE_BUF, "%.3fMHz BW:%.1f", _node_prefs->freq, _node_prefs->bw);
    drawLine(3, DisplayDriver::LIGHT, tmp);

    snprintf(tmp, LINE_BUF, "SF%d CR:%d TX:%ddBm",
             _node_prefs->sf, _node_prefs->cr, _node_prefs->tx_power_dbm);
    drawLine(4, DisplayDriver::LIGHT, tmp);

    uint32_t up = millis() / 1000;
    snprintf(tmp, LINE_BUF, "Up:%luh %lum",
             (unsigned long)(up / 3600), (unsigned long)((up % 3600) / 60));
    drawLine(5, DisplayDriver::LIGHT, tmp);
  }

  void renderGPS(char* tmp) {
    drawLine(0, DisplayDriver::LIGHT, "-- GPS/NTP --");

    LocationProvider* loc = sensors.getLocationProvider();
    bool fix = loc && loc->isValid();

    snprintf(tmp, LINE_BUF, "Stratum:%d", static_cast<int>(currentStratum(fix, _clock_synchronized)));
    drawLine(1, !fix && !_clock_synchronized ? DisplayDriver::RED : DisplayDriver::LIGHT, tmp);

    if (fix) {
      _last_lat = (double)loc->getLatitude() / 1000000.0;
      _last_lon = (double)loc->getLongitude() / 1000000.0;
      _last_alt = (double)loc->getAltitude() / 1000.0;
      _has_last_position = true;
    }

    if (_has_last_position) {
      snprintf(tmp, LINE_BUF, "Lat:%.4f", _last_lat);
      drawLine(2, DisplayDriver::LIGHT, tmp);
      snprintf(tmp, LINE_BUF, "Lon:%.4f", _last_lon);
      drawLine(3, DisplayDriver::LIGHT, tmp);
      snprintf(tmp, LINE_BUF, "Alt:%.1fm", _last_alt);
      drawLine(4, DisplayDriver::LIGHT, tmp);
    } else {
      drawLine(2, DisplayDriver::RED, "No GPS fix");
      drawLine(3, DisplayDriver::LIGHT, "Waiting...");
      drawLine(4, DisplayDriver::LIGHT, "");
    }

    snprintf(tmp, LINE_BUF, "NTP qry:%lu",
             _ntp ? (unsigned long)_ntp->getQueryCount() : 0UL);
    drawLine(5, DisplayDriver::LIGHT, tmp);
  }
};
