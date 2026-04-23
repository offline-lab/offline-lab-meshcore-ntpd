# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
WIFI_SSID=mynet WIFI_PASS=secret pio run              # build
WIFI_SSID=mynet WIFI_PASS=secret pio run -t upload     # flash via USB
pio device monitor                                      # serial console
```

Output: `.pio/build/heltec_wireless_tracker/firmware.bin`

## Architecture

PlatformIO project using MeshCore as a library dependency. Board-agnostic source
code — hardware specifics live in `platformio.ini` env sections.

### Source files (board-agnostic)
- `src/main.cpp` — setup/loop, wires all modules
- `src/ntp_server.h` — UDP NTP responder (stratum 1/2/16, ref ID "GPS"), 48-byte packets
- `src/web_server.h` — AsyncWebServer: `/` dashboard, JSON API
- `src/wifi_manager.h` — WiFi STA connection using build-time SSID/PASS, non-blocking reconnect
- `src/display.h` — TFT status display (uses MeshCore's DisplayDriver abstraction)
- `src/time_broadcaster.h` — UDP TIME_ANNOUNCE discovery broadcast (port 5354, every 16s)

### HTTP API endpoints
- `/api/status` — Full device status (time, GPS, NTP, network)
- `/api/location` — GPS position only (for map integrations)
- `/api/time` — NTP source info (unix, ISO-8601 UTC, stratum, fix status)
- `/api/mesh` — Mesh/LoRa config and neighbors

### Build plumbing
- `platformio.ini` — `[timesyncd]` base section + per-board `[env:...]` sections
- `extra_script.py` — Reads `MC_VARIANT` from build flags, adds variant/src/example include paths + compiles ed25519 from MeshCore

### Key dependency: MeshCore
- Pulled via `lib_deps = https://github.com/meshcore-dev/MeshCore.git`
- `MC_VARIANT` tells `build_as_lib.py` which variant to compile (pins, peripherals, GPS, display driver)
- `BUILD_EXAMPLE=simple_repeater` compiles upstream's `MyMesh.cpp` and `RateLimiter.h` directly from the installed library (no local copy)
- `EXCLUDE_FROM_EXAMPLE` removes `main.cpp` and `UITask.cpp` (we have our own)
- Board globals from MeshCore: `rtc_clock`, `sensors`, `board`, `display`, `node_lat/lon/altitude`

### Adding a new board
See `CONTRIBUTING.md`. Copy an existing env section, change `MC_VARIANT`, pins, radio config.

### WiFi
SSID and password are compiled in via `WIFI_SSID` and `WIFI_PASS` env vars.
Build fails with a clear `static_assert` if either is empty.
