# ESP32 DMD

ESP32 firmware for a HUB75 RGB LED panel driven by Raspydarts over MQTT:
a port of **Raspy2DMD** (the Raspberry Pi DMD renderer) to a single ESP32 board.

It understands the same `action|arg|...` MQTT messages as Raspy2DMD 1.5.4.27: text with movements and
colours, scores with special moves and animations, GIFs and images, the clock, the attract mode and text
carousel, OpenWeatherMap weather and forecast, EDF Tempo days, and the `conf` settings. It also adds a few
ESP32-only effects (fireworks, plasma, rainbow and typewriter text, ...).

The full command table, settings and differences from the Pi are in [docs/PORTING.md](docs/PORTING.md).

## Hardware

- ESP32 dev board (4 MB flash, no PSRAM needed)
- HUB75 panel, 64x32 by default (`DMDRenderer.cols`, `rows`, `led_chain` change it; 64-row panels need
  `-DE_PIN=<gpio>`)
- Wiring: see the pin table in `src/matrix/Hub75_Matrix.h` (R1 25, G1 26, B1 27, R2 21, G2 22, B2 23,
  A 12, B 16, C 17, D 18, CLK 15, LAT 32, OE 33)

## Build and flash

```bash
pio run -t upload        # firmware
pio device monitor       # logs at 115200 baud
```

The first boot (and the first boot after the partition change) opens a Wi-Fi access point
`DMD_CONFIG_WIFI`: connect to it and enter your Wi-Fi network, the MQTT broker (usually `raspydarts.local`),
the MQTT topic and the hostname.

## Using it

- Raspydarts drives it over MQTT. By hand: `mosquitto_pub -h raspydarts.local -t <topic> -m 'msg|Hello|3'`.
- Web pages on `http://<board-ip>/`:
  - `/settings`: every Raspy2DMD setting (same keys as `Raspy2DMD.cfg` and `conf|Section|key:value`)
  - `/files`: upload GIFs, images, carousel texts, `effets.txt`... into the board's 900 KB LittleFS
  - `/config`: Wi-Fi portal values (MQTT broker, topic, hostname)

## Code layout

| Folder | What |
|---|---|
| `src/core` | Board-independent logic: protocol, scheduling, text layout, motions, effects maths, clock, media rules, weather/Tempo logic, settings schema. Unit tested on the host. |
| `src/net` | JSON parsers for OpenWeatherMap and EDF Tempo (ArduinoJson) |
| `src/render` | Scenes drawn on the panel: text, effects, clock, GIF/PNG, weather |
| `src/util` | Firmware services: MQTT, Wi-Fi, web server, settings, storage, attract mode, NTP, online fetches |
| `src/matrix` | HUB75 DMA driver wrapper |
| `test/host` | Host unit tests |

## Tests

```bash
make -C test/host                                         # core tests (ASan/UBSan) + C++11 compile check
make -C test/host json ARDUINOJSON=<ArduinoJson>/src      # JSON parser tests
```

CI (`.github/workflows/build.yml`) runs both and builds the firmware on every pull request.
