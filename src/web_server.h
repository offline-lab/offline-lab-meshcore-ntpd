#pragma once

#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <target.h>

struct MeshInfo {
  char node_name[32];
  float freq;
  float bw;
  uint8_t sf;
  uint8_t cr;
  int8_t tx_power;
  bool fwd_enabled;
  char neighbors[160];
};

typedef void (*FillMeshInfoFn)(MeshInfo*);

class TimeWebServer {
  AsyncWebServer _server;
  uint32_t (*_get_ntp_queries)() = nullptr;
  FillMeshInfoFn _fill_mesh = nullptr;
  unsigned long _next_update = 0;

  struct {
    uint32_t unix_time;
    bool gps_fix;
    double latitude;
    double longitude;
    double altitude;
    long satellites;
    uint32_t ntp_queries;
    uint32_t uptime_secs;
    char ip[16];

    char status_json[320];
    char location_json[128];
    char time_json[192];
    char mesh_json[512];
    bool clock_synchronized = false;
  } _cache = {};

public:
  TimeWebServer() : _server(80) {}

  void begin(uint32_t (*get_ntp_queries)(), FillMeshInfoFn fill_mesh = nullptr) {
    _get_ntp_queries = get_ntp_queries;
    _fill_mesh = fill_mesh;

    _server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
      request->send(200, "text/html", buildDashboard());
    });

    _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
      request->send(200, "application/json", _cache.status_json);
    });

    _server.on("/api/location", HTTP_GET, [this](AsyncWebServerRequest* request) {
      request->send(200, "application/json", _cache.location_json);
    });

    _server.on("/api/time", HTTP_GET, [this](AsyncWebServerRequest* request) {
      request->send(200, "application/json", _cache.time_json);
    });

    _server.on("/api/mesh", HTTP_GET, [this](AsyncWebServerRequest* request) {
      request->send(200, "application/json", _cache.mesh_json);
    });

    _server.begin();
  }

  void setClockSynchronized(bool v) { _cache.clock_synchronized = v; }

  void loop() {
    unsigned long now_ms = millis();
    if (now_ms < _next_update) return;
    _next_update = now_ms + 2000;

    LocationProvider* loc = sensors.getLocationProvider();
    bool fix = loc && loc->isValid();

    _cache.gps_fix = fix;
    if (fix) {
      _cache.latitude = (double)loc->getLatitude() / 1000000.0;
      _cache.longitude = (double)loc->getLongitude() / 1000000.0;
      _cache.altitude = (double)loc->getAltitude() / 1000.0;
      _cache.satellites = loc->satellitesCount();
    }

    time_t t;
    time(&t);
    _cache.unix_time = (uint32_t)t;
    _cache.uptime_secs = now_ms / 1000UL;
    if (_get_ntp_queries) _cache.ntp_queries = _get_ntp_queries();

    strncpy(_cache.ip, WiFi.localIP().toString().c_str(), sizeof(_cache.ip) - 1);
    _cache.ip[sizeof(_cache.ip) - 1] = 0;

    rebuildJson();
  }

private:
  static void jsonEscape(char* dst, const char* src, int dst_size) {
    int j = 0;
    for (int i = 0; src[i] && j < dst_size - 2; i++) {
      if (src[i] == '\n') { dst[j++] = '\\'; dst[j++] = 'n'; }
      else if (src[i] == '\r') { /* skip */ }
      else if (src[i] == '"') { dst[j++] = '\\'; dst[j++] = '"'; }
      else if (src[i] == '\\') { dst[j++] = '\\'; dst[j++] = '\\'; }
      else if ((unsigned char)src[i] < 0x20) { /* skip control chars */ }
      else { dst[j++] = src[i]; }
    }
    dst[j] = 0;
  }

  void rebuildJson() {
    snprintf(_cache.location_json, sizeof(_cache.location_json),
      "{\"fix\":%s,\"satellites\":%ld,"
      "\"latitude\":%.6f,\"longitude\":%.6f,\"altitude\":%.1f}",
      _cache.gps_fix ? "true" : "false",
      _cache.satellites,
      _cache.latitude, _cache.longitude, _cache.altitude);

    struct tm* tm = gmtime((time_t*)&_cache.unix_time);
    char iso[32];
    strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%SZ", tm);

    snprintf(_cache.time_json, sizeof(_cache.time_json),
      "{\"unix\":%lu,\"utc\":\"%s\",\"gps_fix\":%s,"
      "\"stratum\":%d,\"reference\":\"%s\",\"ntp_queries\":%lu}",
      (unsigned long)_cache.unix_time, iso,
      _cache.gps_fix ? "true" : "false",
      static_cast<int>(currentStratum(_cache.gps_fix, _cache.clock_synchronized)),
      _cache.clock_synchronized ? "GPS" : "",
      (unsigned long)_cache.ntp_queries);

    snprintf(_cache.status_json, sizeof(_cache.status_json),
      "{\"time\":%lu,\"gps_fix\":%s,\"satellites\":%ld,"
      "\"latitude\":%.6f,\"longitude\":%.6f,\"altitude\":%.1f,"
      "\"ntp_queries\":%lu,\"uptime\":%lu,\"ip\":\"%s\"}",
      (unsigned long)_cache.unix_time,
      _cache.gps_fix ? "true" : "false",
      _cache.satellites,
      _cache.latitude, _cache.longitude, _cache.altitude,
      (unsigned long)_cache.ntp_queries,
      (unsigned long)_cache.uptime_secs,
      _cache.ip);

    if (_fill_mesh) {
      MeshInfo mi = {};
      mi.neighbors[0] = 0;
      _fill_mesh(&mi);
      char name_esc[64];
      jsonEscape(name_esc, mi.node_name, sizeof(name_esc));
      char nei_esc[320];
      jsonEscape(nei_esc, mi.neighbors, sizeof(nei_esc));
      snprintf(_cache.mesh_json, sizeof(_cache.mesh_json),
        "{\"node_name\":\"%s\",\"freq\":%.3f,\"bw\":%.1f,"
        "\"sf\":%d,\"cr\":%d,\"tx_power\":%d,\"forwarding\":%s,"
        "\"neighbors\":\"%s\"}",
        name_esc, mi.freq, mi.bw,
        (int)mi.sf, (int)mi.cr, (int)mi.tx_power,
        mi.fwd_enabled ? "true" : "false",
        nei_esc);
    } else {
      snprintf(_cache.mesh_json, sizeof(_cache.mesh_json), "{}");
    }
  }

  static String buildDashboard() {
    return R"rawhtml(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>GPS TimeServer</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:monospace;background:#0d1117;color:#c9d1d9;padding:1em;max-width:700px;margin:0 auto}
h1{color:#58a6ff;font-size:1.2em;margin-bottom:.5em}
h2{color:#8b949e;font-size:.9em;margin:1em 0 .3em;text-transform:uppercase;letter-spacing:.1em;border-bottom:1px solid #21262d;padding-bottom:.3em}
.card{background:#161b22;border:1px solid #30363d;border-radius:6px;padding:.8em;margin:.5em 0}
.row{display:flex;justify-content:space-between;padding:.2em 0;border-bottom:1px solid #21262d}
.row:last-child{border:none}
.lbl{color:#8b949e;font-size:.85em}
.val{color:#58a6ff;font-weight:bold}
.val.ok{color:#3fb950}
.val.err{color:#f85149}
a{color:#58a6ff}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:.5em}
small{color:#8b949e;font-size:.75em}
.nei{font-size:.8em;padding:.1em .4em;background:#21262d;border-radius:3px;margin:.1em;display:inline-block}
</style>
<script>
function $(id){return document.getElementById(id)}
function html(id,v){$(id).textContent=v}
function cls(id,c){$(id).className='val '+c}
function fmtUp(s){var h=Math.floor(s/3600),m=Math.floor((s%3600)/60);return h>0?h+'h '+m+'m':m+'m'}

function updateStatus(){
  fetch('/api/status').then(r=>r.json()).then(function(s){
    var t=new Date(s.time*1000).toISOString().replace('T',' ').replace('Z',' UTC');
    var f=s.gps_fix;
    html('utime',t);cls('utime',f?'ok':'err');
    html('gfix',f?'YES':'NO');cls('gfix',f?'ok':'err');
    html('gsat',''+s.satellites);
    html('glat',''+s.latitude);html('glon',''+s.longitude);html('galt',s.altitude+' m');
    html('nq',''+s.ntp_queries);html('up',fmtUp(s.uptime));html('ip',s.ip);
    html('stratum',f?'1 (GPS)':'16 (no fix)');cls('stratum',f?'ok':'err');
  }).catch(function(e){console.error('status:',e)});
}

function updateMesh(){
  fetch('/api/mesh').then(r=>r.json()).then(function(m){
    if(!m||!m.node_name)return;
    html('mname',m.node_name);
    html('mradio',m.freq+' MHz BW:'+m.bw+' SF'+m.sf+' CR:'+m.cr+' TX:'+m.tx_power+'dBm');
    html('mfwd',m.forwarding?'ON':'OFF');cls('mfwd',m.forwarding?'ok':'err');
    var nel=$('mnei');
    if(m.neighbors&&m.neighbors.length>0){
      var lines=m.neighbors.split('\n').filter(function(n){return n.indexOf(':')!==-1});
      if(lines.length>0){
        nel.innerHTML=lines.map(function(n){
          var p=n.split(':');
          return '<span class="nei">'+p[0]+' <small>'+p[1]+'s '+p[2]+'dB</small></span>';
        }).join('');
      }else{nel.innerHTML='<span class="lbl">none</span>';}
    }else{nel.innerHTML='<span class="lbl">none</span>';}
  }).catch(function(){});
}

function update(){updateStatus();updateMesh();}
setInterval(update,3000);update();
</script></head>
<body>
<h1>MeshCore GPS TimeServer</h1>

<h2>NTP / Time</h2>
<div class="card">
<div class="row"><span class="lbl">UTC</span><span class="val" id="utime">--</span></div>
<div class="row"><span class="lbl">Stratum</span><span class="val" id="stratum">--</span></div>
<div class="row"><span class="lbl">NTP Queries</span><span class="val" id="nq">0</span></div>
</div>

<h2>GPS</h2>
<div class="card">
<div class="row"><span class="lbl">Fix</span><span class="val" id="gfix">--</span></div>
<div class="row"><span class="lbl">Satellites</span><span class="val" id="gsat">0</span></div>
<div class="grid">
<div><span class="lbl">Lat</span><div class="val" id="glat">--</div></div>
<div><span class="lbl">Lon</span><div class="val" id="glon">--</div></div>
</div>
<div class="row"><span class="lbl">Altitude</span><span class="val" id="galt">--</span></div>
</div>

<h2>Mesh / LoRa</h2>
<div class="card">
<div class="row"><span class="lbl">Node</span><span class="val" id="mname">--</span></div>
<div class="row"><span class="lbl">Forwarding</span><span class="val" id="mfwd">--</span></div>
<div class="row"><span class="lbl">Radio</span><span class="val" id="mradio">--</span></div>
<div class="row"><span class="lbl">Neighbors</span><span id="mnei"><span class="lbl">--</span></span></div>
</div>

<h2>System</h2>
<div class="card">
<div class="row"><span class="lbl">Uptime</span><span class="val" id="up">--</span></div>
<div class="row"><span class="lbl">IP</span><span class="val" id="ip">--</span></div>
</div>

</body></html>)rawhtml";
  }
};
