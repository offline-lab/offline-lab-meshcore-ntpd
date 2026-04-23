# offline-lab-meshcore-ntpd

This is a small wrapper around [MeshCore](https://github.com/meshcore-dev/MeshCore) that offers a repeater 
and some additional time and location functionality on the side. 

In offline environments, I don't have access to an NTPd service, which come with its own setbacks. 
To work around this, this small sketch, running on a Heltec wireless tracker, uses the time from a GPS receiver 
and offers this as a semi-accurate ntp daemon. (I have all the time, but it's just not exactly on time)

Stratum 1 with live GPS fix, stratum 2 in holdover (GPS was synced this session but was lost since), stratum 16 if never synced. 

Also broadcasts `TIME_ANNOUNCE` via UDP for [disco](https://github.com/offline-lab/disco).

Tested on the [Heltec Wireless Tracker](https://heltec.org/project/wireless-tracker/) (ESP32-S3 + UC6580 GPS + SX1262 LoRa + ST7735 TFT). 
See [CONTRIBUTING.md](CONTRIBUTING.md) to add more boards.

This project was created with several LLMs and coding harnesses.

## Building & flashing

Requires [PlatformIO](https://platformio.org/). WiFi credentials are compile-time:

```bash
WIFI_SSID=mynet WIFI_PASS=secret pio run              # build
WIFI_SSID=mynet WIFI_PASS=secret pio run -t upload    # flash via USB
pio device monitor                                    # serial console
```

After initial USB flash, OTA over WiFi:

```bash
pio run --upload-port time.local -t upload
pio run --upload-port 192.168.8.123 -t upload
```

Discoverable as `time.local` via mDNS. OTA uses ArduinoOTA (port 3232), no web upload.

## NTP clients

```bash
# chrony
echo "server time.local iburst" >> /etc/chrony/chrony.conf

# systemd-timesyncd
echo -e "[Time]\nNTP=time.local" > /etc/systemd/timesyncd.conf

# macOS
sudo sntp -sS time.local
```

## HTTP API

Dashboard at `http://<device-ip>/`. JSON endpoints:

| Endpoint | Returns |
|----------|---------|
| `/api/status` | Time, GPS, NTP queries, uptime, IP |
| `/api/time` | Unix, ISO-8601 UTC, stratum, reference |
| `/api/location` | Lat/lon/alt, satellites (last known during holdover) |
| `/api/mesh` | LoRa config, neighbors |

## Serial CLI

USB serial at 115200 baud. Full MeshCore CLI available — see
[CLI reference](https://github.com/meshcore-dev/MeshCore/blob/main/docs/cli_commands.md).

| Command | Description |
|---------|-------------|
| `get radio` / `set radio F,BW,SF,CR` | Radio params |
| `get tx` / `set tx N` | TX power (dBm) |
| `get name` / `set name X` | Node name |
| `neighbors` / `discover.neighbors` | List/probe neighbors |
| `gps` / `gps on` / `gps off` | GPS state and hardware |
| `gps sync` | Sync RTC from GPS |
| `gps setloc` | Set advertised location from GPS |
| `get repeat` / `set repeat on|off` | Packet forwarding |
| `stats-core` / `stats-radio` / `stats-packets` | Device stats |
| `log start` / `log stop` | Packet logging |
| `reboot` / `erase` | Reboot / factory reset |

## Display

3 pages on user button press: status, mesh config, GPS/NTP detail.
Auto-off after 60s. Refreshes every 2s, only redraws changed lines.

## Discovery broadcast

`TIME_ANNOUNCE` JSON on UDP port 5354 every 16s while clock is synchronized.
Compatible with [disco](https://github.com/offline-lab/disco).

```bash
nc -ul 5354
```

## LoRa defaults (EU868)

| Parameter | Value |
|-----------|-------|
| Frequency | 869.618 MHz |
| Bandwidth | 62.5 kHz |
| Spreading Factor | 8 |
| TX Power | 22 dBm |

Change via `platformio.ini` build flags or serial CLI (`set radio`, `set tx`).

## Power

~190mA with display on, ~160mA off. Display auto-off saves ~30mA.
WiFi modem sleep is disabled intentionally — adds 100-300ms variable NTP latency.

## Adding boards

See [CONTRIBUTING.md](CONTRIBUTING.md). Copy an env section in `platformio.ini`,
set `MC_VARIANT` and pin definitions for your board.

## License

MIT. MeshCore: MIT — https://github.com/meshcore-dev/MeshCore
