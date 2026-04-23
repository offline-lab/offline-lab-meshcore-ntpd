# Adding a New Board

Copy an existing env section in `platformio.ini` and adjust for your board.
This project is board-agnostic — all hardware specifics live in the env config.

## What you need

1. A board supported by [MeshCore](https://github.com/meshcore-dev/MeshCore) with a
   matching variant in `variants/`
2. GPS (optional — time server degrades to stratum 16 without it)
3. WiFi (required for NTP/HTTP)
4. A display (optional — web dashboard works without it)

## Steps

### 1. Find your MeshCore variant name

Look in MeshCore's `variants/` directory for your board. The directory name is
the variant name (e.g. `heltec_tracker`, `lilygo_tbeam`, `heltec_v3`).

### 2. Add an env section

Copy an existing `[env:...]` section and change:

- Section name (e.g. `[env:lilygo_tbeam]`)
- `board` — PlatformIO board ID
- `-D MC_VARIANT=your_variant` — must match the MeshCore variant directory name
- Pin definitions — from your board's `variants/.../target.h`
- Radio chip and config — match your hardware (SX1262, SX1276, etc.)
- Display class — match your display type (`ST7735Display`, `OLEDDisplay`, etc.)
- `lib_deps` — add/remove display and GPS libraries as needed

### 3. Build

```bash
WIFI_SSID=mynet WIFI_PASS=secret pio run -e your_env_name
```

### 4. Test and PR

Flash, verify NTP/HTTP/mesh work, then open a pull request.

## What not to change

- `src/*.h` and `src/main.cpp` are board-agnostic — they use MeshCore globals
  (`board`, `sensors`, `display`, `rtc_clock`) provided by the variant
- `extra_script.py` automatically picks up the variant from `MC_VARIANT`
- The `[timesyncd]` base section contains shared config — extend it, don't modify it

## Example: minimal env

```ini
[env:my_board]
extends = timesyncd
board = esp32-s3-devkitc-1

build_flags =
  ${timesyncd.build_flags}
  -D MC_VARIANT=my_variant
  -D USE_SX1262
  -D RADIO_CLASS=CustomSX1262
  -D WRAPPER_CLASS=CustomSX1262Wrapper
  -D LORA_FREQ=868.0
  -D LORA_BW=125.0
  -D LORA_SF=7
  -D LORA_TX_POWER=20
  ; ... pin definitions from your variant's target.h ...
  -D ENV_INCLUDE_GPS=1
  -D DISPLAY_CLASS=ST7735Display

lib_deps =
  ${timesyncd.lib_deps}
  stevemarple/MicroNMEA @ ^2.0.6
  adafruit/Adafruit ST7735 and ST7789 Library @ ^1.11.0
```
