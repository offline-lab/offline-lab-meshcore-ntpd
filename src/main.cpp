#include <Arduino.h>
#include <Mesh.h>
#include <SPIFFS.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>

#include <MyMesh.h>
#include "wifi_manager.h"
#include "ntp_server.h"
#include "time_broadcaster.h"
#include "web_server.h"
#include "display.h"

static bool services_started = false;

static UITask ui_task(display);

StdRNG fast_rng;
SimpleMeshTables tables;

MyMesh the_mesh(board, radio_driver, *new ArduinoMillis(), fast_rng, rtc_clock, tables);

SimpleWiFiManager wifi_mgr;
NTPServer ntp_server;
TimeBroadcaster time_broadcaster;
TimeWebServer web_server;

static uint32_t get_ntp_queries() { return ntp_server.getQueryCount(); }
static char command[160];

static void fill_mesh_info(MeshInfo* mi) {
  NodePrefs* p = the_mesh.getNodePrefs();
  strncpy(mi->node_name, p->node_name, sizeof(mi->node_name) - 1);
  mi->freq = p->freq;
  mi->bw = p->bw;
  mi->sf = p->sf;
  mi->cr = p->cr;
  mi->tx_power = p->tx_power_dbm;
  mi->fwd_enabled = !p->disable_fwd;
  the_mesh.formatNeighborsReply(mi->neighbors);
}

static void startServices() {
  if (services_started) return;
  ntp_server.begin();
  time_broadcaster.begin();
  web_server.begin(get_ntp_queries, fill_mesh_info);
  ArduinoOTA.setHostname("time");
  ArduinoOTA.begin();
  if (MDNS.begin("time")) {
    MDNS.addService("ntp", "udp", 123);
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("arduino", "tcp", 3232);
  }
  services_started = true;
}

static void halt() { while (1) ; }

void setup() {
  Serial.begin(115200);
  delay(1000);
  board.begin();

  if (display.begin()) {
    display.startFrame();
    display.setCursor(0, 0);
    display.print("Starting...");
    display.endFrame();
  }

  if (!radio_init()) {
    Serial.println("Radio init failed!");
    halt();
  }

  fast_rng.begin(radio_get_rng_seed());

  SPIFFS.begin(true);
  IdentityStore store(SPIFFS, "/identity");
  if (!store.load("_main", the_mesh.self_id)) {
    Serial.println("Generating new keypair");
    the_mesh.self_id = radio_new_identity();
    int count = 0;
    while (count < 10 && (the_mesh.self_id.pub_key[0] == 0x00 || the_mesh.self_id.pub_key[0] == 0xFF)) {
      the_mesh.self_id = radio_new_identity();
      count++;
    }
    store.save("_main", the_mesh.self_id);
  }

  Serial.print("Repeater ID: ");
  mesh::Utils::printHex(Serial, the_mesh.self_id.pub_key, PUB_KEY_SIZE);
  Serial.println();

  command[0] = 0;
  sensors.begin();
  the_mesh.begin(&SPIFFS);

#if ENV_INCLUDE_GPS
  if (!the_mesh.getNodePrefs()->gps_enabled) {
    the_mesh.handleCommand(0, (char*)"gps on", command);
  }
#endif

  board.setInhibitSleep(true);
  if (wifi_mgr.begin()) {
    Serial.print("WiFi connected: ");
    Serial.println(WiFi.localIP());
    startServices();
  } else {
    Serial.println("WiFi not connected, will retry");
  }

  ui_task.begin(the_mesh.getNodePrefs(), FIRMWARE_BUILD_DATE, FIRMWARE_VERSION,
                &ntp_server, &wifi_mgr);

  the_mesh.sendSelfAdvertisement(16000, false);
}

void loop() {
  int len = strlen(command);
  while (Serial.available() && len < (int)sizeof(command) - 1) {
    char c = Serial.read();
    if (c != '\n') {
      command[len++] = c;
      command[len] = 0;
      Serial.print(c);
    }
    if (c == '\r') break;
  }
  if (len == (int)sizeof(command) - 1) {
    command[sizeof(command) - 1] = '\r';
  }
  if (len > 0 && command[len - 1] == '\r') {
    Serial.print('\n');
    command[len - 1] = 0;

    char reply[160];
    the_mesh.handleCommand(0, command, reply);
    if (reply[0]) {
      Serial.print("  -> ");
      Serial.println(reply);
    }
    command[0] = 0;
  }

  the_mesh.loop();
  sensors.loop();
  wifi_mgr.loop();

  LocationProvider* loc = sensors.getLocationProvider();
  bool gps_has_fix = loc && loc->isValid();
  static bool gps_had_fix = false;
  if (gps_has_fix) gps_had_fix = true;
  bool clock_synchronized = gps_has_fix || gps_had_fix;

  if (wifi_mgr.isConnected()) {
    if (!services_started) startServices();
    ntp_server.loop(rtc_clock.getCurrentTime(), clock_synchronized, gps_has_fix);
    time_broadcaster.loop(rtc_clock.getCurrentTime(), clock_synchronized);
    web_server.setClockSynchronized(clock_synchronized);
    web_server.loop();
    ArduinoOTA.handle();
  }

  ui_task.loop();
  ui_task.setClockSynchronized(clock_synchronized);
  rtc_clock.tick();
}
