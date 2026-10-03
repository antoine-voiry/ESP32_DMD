# Raspy2DMD → ESP32 porting status

Reference: Raspy2DMD 1.5.4.27 (`ServerRaspy2DMD.py`, `bin/DMDRenderer.py`, `bin/DMDRenderer_SpecialsMoves.py`,
`config/DMDRenderer_Config.py`).

## Architecture

| Python (Raspberry Pi) | ESP32 |
|---|---|
| `on_message` + `FilterMessages` | `MQTTHelper` (FIFO queue) + `MessageFilter` → `core/Protocol` |
| `UnstackMessages` dispatcher | `MessageHandler` |
| `RenderText` and friends, one thread each | `DMDRenderer` queues `Scene`s on `core/SceneRunner`, advanced from `loop()` |
| `Stop()` / `_stopAffichage` flags | `SceneRunner::interrupt()` |
| `time.sleep(N)` after `msg\|text\|N` | per-scene hold: the next message waits N s, the panel keeps updating |
| PIL text image + `SetImage` loops | `render/TextScene`: 1-bit image + `core/Motion` frame timeline |
| `DMDRenderer_SpecialsMoves` | `core/SpecialMoves` |

`src/core` has no Arduino dependency and is unit tested on the host: `make -C test/host`.
The JSON parsers in `src/net` are tested with ArduinoJson: `make -C test/host json ARDUINOJSON=<ArduinoJson>/src` (CI does this).

## Commands

Payloads are `action|arg|arg...`. A payload without `|`, an unknown action, or an invalid score is dropped.
A valid payload interrupts the animation on screen, even if its arguments turn out to be missing.

| Command | Arguments | Status |
|---|---|---|
| `msg` | `text[\|holdSec]` (empty text shows `-Vide-`) | ✅ |
| `score` | `S20 - T20 - X[\|holdSec]` | ✅ GIF from `/scores/<last dart>/`, then `/specialsmoves/<MOVE>/` GIF or the move's name, then the darts |
| `msgmove` | `text\|sens[\|holdSec]` | ✅ |
| `msgmovebcl` | `text\|sens\|iterations[\|holdSec]` | ✅ (original slept `iterations` s, a typo; we use `holdSec`) |
| `msgcolor` | `text\|r;g;b\|r;g;b` | ✅ |
| `conf` | `Section\|key:value\|...` | ✅ stored in `config.json` and read live (see *Settings*); panel size and standalone restart the board |
| `rebt` | – | ✅ `ESP.restart()` |
| `shutdwn` | – | ✅ blank panel + deep sleep (power cycle to restart) |
| `testFont` | `font.ttf` | ⚠️ sample text with built-in fonts (no TrueType on ESP32) |
| `soundeffet` | `text\|gif\|sound` | ✅ GIF with text, text or GIF (sound ignored: no audio) |
| `sound` | `file` | ❌ no audio output on this board |
| `msgimg` | `text\|png` | ✅ text centred over a PNG (looked up in `/patterns`) |
| `time` | `start\|stop` | ✅ date and/or time per `ClockRenderer`, over `/patterns/<clockBackgroundImage>` |
| `waiter` | `start\|stop\|pause\|resume` | ✅ plays `Running.scrollOrder` while idle (`pause` = `stop`) |
| `msgcarrou` | `start\|stop` | ✅ random text file from SPIFFS `/textes/` |
| `testPattern` | `pattern.png` | ✅ clock preview over that pattern |
| `rldconf` | – | ✅ re-applies text style, brightness and timezone |
| `gif` | `name[\|holdSec]` | ✅ looked up in `/gifs` (or as a mapped Pi path), played once |
| `gifPath` | `/Medias/...[\|holdSec]` | ✅ Pi path mapped to LittleFS (`/Medias/Gifs/x.gif` → `/gifs/x.gif`) |
| `gifText` | `gif\|text[\|holdSec]` | ✅ text over the GIF (the original read the hold from a missing 4th argument) |
| `img` | `name[\|holdSec]` | ✅ PNG from `/images`; `WELK.OME` = `/images/Raspy2DMD.png` |
| `rand` | `gif\|img[\|holdSec]` | ✅ random file, exclusions applied |
| `demo` | `gif` | ✅ every GIF, each preceded by its name |
| `effet` | `id` | ✅ from `/effets.txt` (`id\|name\|text\|gif\|sound` per line) instead of MariaDB |
| `excludeFolder`, `excludeFile` | `name\|path` | ✅ toggles, saved in `/exclusions.txt` |
| `meteo` | – | ✅ current weather: icon, temperature, wind arrow and speed |
| `meteoPrevi` | – | ✅ forecast for `prevision` days, one page per day (2 slots per page on 64 px, 4 on 128 px) |
| `edfJoursTempo` | – | ✅ today's and tomorrow's Tempo colour (api-couleur-tempo.fr) |
| `owmzc`, `fllcn` | – | ✅ zip code → city name, lat, lon (stored in `OpenWeatherMap`) |
| `perf` | – | ✅ board status: temperature, CPU MHz, free RAM, uptime, Wi-Fi signal and IP |
| `receipconf` | – | ✅ publishes every setting as `Section:key:value` on `Running.raspydartscanal` when `Running.resptoraspydarts` is 1 |

`sens`: `left`, `right`, `up`, `down`, `rotate`, `antirotate`, `flip`, `twirl`.

## Attract mode, clock and carousel

- `Running.scrollOrder` codes playable on the ESP32: `1` random GIF, `2` random image (4 s), `T` clock, `4` text
  carousel, `M` weather, `P` forecast, `E` EDF Tempo, `S` board status, and `F` random `fx` animation (ESP32 extension).
- `Running.attract_mode` (seconds, default 0 = off): attract mode starts by itself after that long without a message.
  Any message stops it.
- Time comes from NTP; `ClockRenderer.timezone` takes the IANA name used on the Pi (`Europe/Paris`, ...) or a POSIX TZ.
  Date names follow `format_affichage` (`fr_FR` or `en_*`), e.g. `03 oct. 2026` with the default `%d %b %Y`.
- `DMDRenderer.brightnesshours` (24 values) is applied every hour once the clock is synchronised.
- Carousel files: folder `/textes/` (any sub-folder), same format as on the Pi: first line `;`-separated options
  (`DG`, `GD`, `HB`, `BH`, `ROT`, `ARO`, `FLI`, `TWI`, `A` random, `ITn` repeats), then the message.
  Static texts are held 4 s.

## Media files

Everything lives on the board's LittleFS partition (~900 KB) and can be managed from the web page `http://<board>/files`
(upload into a folder, delete). The Pi's `/Medias/<Dir>/` folders become lower-case folders at the root:

| Pi | ESP32 | Used by |
|---|---|---|
| `/Medias/Gifs/` | `/gifs/` | `gif`, `rand\|gif`, `demo`, attract `1` |
| `/Medias/Images/` | `/images/` | `img`, `rand\|img`, attract `2`, start-up logo `Raspy2DMD.png` |
| `/Medias/Scores/<dart>/` | `/scores/<dart>/` | `score` (last dart that is not `X`) |
| `/Medias/SpecialsMoves/<MOVE>/` | `/specialsmoves/<MOVE>/` | `score` special moves |
| `/Medias/Patterns/` | `/patterns/` | clock background, `msgimg`, `testPattern` |
| `/Medias/Textes/` | `/textes/` | carousel |

GIFs and PNGs are shrunk to fit the panel (never enlarged) and centred when `DMDRenderer.center_images` is 1.
Keep them panel-sized: decoding a 640x480 HDMI asset works but is slow and wastes flash.

## Online data

- Requests run on a background task (HTTPS takes 1–3 s), so MQTT and animations never stall; a loading animation
  shows meanwhile, and an error message (no appid, no Wi-Fi, HTTP error) if the request fails.
- Weather answers are cached for `OpenWeatherMap.callevery` minutes, Tempo until the next day, like the Pi's token files.
- Icons: the Pi's PNG sets are used when uploaded to `/meteo/<icon>.png` (OWM codes, e.g. `10d.png`) and
  `/edfjourstempo/<code>.png`; otherwise icons, wind arrow and Tempo colours are drawn by the firmware.
- Wind speed is converted from m/s to km/h (the Pi printed the m/s value with a "km/h" label).
- TLS certificates are not verified (no CA store on the board); only public weather data and the OWM key go over it.

## Settings

`conf|Section|key:value` and the web page `/settings` store any Raspy2DMD key in `config.json` (`settings`, keyed
`Section.key`; keys are case-insensitive like configparser). Every key of `Raspy2DMD.cfg` is known
(`src/core/ConfigSchema.cpp`) so `receipconf` reports them all; the ones used by the ESP32:

| Section | Keys |
|---|---|
| `TextRenderer` | `defaultfontcolor` (0,0,255), `picturebackgroundcolor` (0,0,0), `maxcharacter` (22), `maxfontsize` (30) |
| `ClockRenderer` | `clockBackgroundImage` (OldGame.png), `showing_datehours` (2), `timeShow_Date` (2), `timeShow_Hours` (4), `format_date` (`%d %b %Y`), `format_hours` (`%H:%M:%S`), `format_affichage` (fr_FR), `timezone` (Europe/Paris), `defaultfontcolor_clock` (0,0,255), `defaultfontcolor_clockshadow` (255,0,0) |
| `Running` | `scrollOrder` (1,T), `attract_mode` (0), `standalone` (0), `default` (1: show the web address at start-up), `raspydartscanal` (raspydarts/dmd), `resptoraspydarts` (0) |
| `OpenWeatherMap` | `appid` (0 = off), `lat`, `lon`, `zipcode`, `countrycode`, `units` (metric), `lang` (fr), `callevery` (15 min), `seeduring` (4 s), `prevision` (1 day) |
| `DMDRenderer` | `cols` (64), `rows` (32), `led_chain` (1) — applied at start-up, `brightness` (90), `brightnesshours`, `center_images` (1) |

Standalone mode (`Running.standalone = 1`): the attract mode starts at boot and only the commands the Pi accepted in
standalone mode are handled (`msg`, `conf`, `rldconf`, `receipconf`, weather, Tempo, `perf`, exclusions, reboot,
shutdown); the others are ignored without interrupting the display. Unlike the Pi, where one message stopped the
standalone display for good, the attract mode resumes 5 s after a message (or `attract_mode` seconds).

Kept for Raspydarts but without effect on the ESP32 (shown greyed on `/settings`): Pi GPIO/PWM timings, HDMI, sound,
TrueType fonts and clock positions, `Directory` paths (the media folders are fixed, see *Media files*).

## ESP32 extensions

Not part of Raspy2DMD (Raspydarts never sends them), handy from `mosquitto_pub` or Home Assistant.

| Command | Arguments | Effect |
|---|---|---|
| `fx` | `plasma\|fireworks\|stars\|matrix[\|seconds]` | Full-panel animation (default 5 s, max 10 min) |
| `msgfx` | `text\|effect[\|seconds]` | Text effect: `solid`, `rainbow`, `wave`, `typewriter`, `sparkle`; or a background name for rainbow text over that animation |

Special moves in `score` are celebrated: fireworks with rainbow text for `MAXIMUM_TON_80`, `BLACK_HAT…`, `RED_HAT`,
`HAT_TRICK` and `CHAMPAGNE_BREAKFAST`; sparkling text for the others (same 2 s as the original's plain text).

## Differences from the original

- Fonts: Adafruit GFX bitmap fonts (FreeSansBold 18/12/9 pt, then the built-in 6x8 font at x2 and x1),
  largest that fits, instead of a shrinking TrueType font. Text is ASCII only: accents are stripped (`é` → `e`).
- `showing_datehours = 1` (the `GetConfig()` fallback) showed nothing on the Pi; it alternates date and time here.
- Default `led_chain` is 1 (one 64x32 panel) instead of the Pi's 2.
- No audio output: `sound` and the sound part of effects are ignored.
- `rebt` restarts the ESP32; `shutdwn` blanks the panel and deep-sleeps (power cycle to restart).

## Phases

1. **Core engine**: non-blocking renderer, FIFO messages, text and movements, score, colours, brightness, reboot. ✅
2. **Clock and attract mode**: NTP time, `time`, `waiter`, `scrollOrder` playlist, `brightnesshours`, text carousel. ✅
3. **Media**: GIF/PNG from LittleFS, score and special-move animations, effects, exclusions, upload page. ✅
4. **Online data**: OpenWeatherMap, EDF Tempo days, board status. ✅
5. **Settings**: full config schema, `receipconf`/`rldconf`, panel geometry, standalone mode, settings web page. ✅
