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

## Commands

Payloads are `action|arg|arg...`. A payload without `|`, an unknown action, or an invalid score is dropped.
A valid payload interrupts the animation on screen, even if its arguments turn out to be missing.

| Command | Arguments | Status |
|---|---|---|
| `msg` | `text[\|holdSec]` (empty text shows `-Vide-`) | ✅ |
| `score` | `S20 - T20 - X[\|holdSec]` | ✅ text + special move name; score/special-move GIFs in phase 3 |
| `msgmove` | `text\|sens[\|holdSec]` | ✅ |
| `msgmovebcl` | `text\|sens\|iterations[\|holdSec]` | ✅ (original slept `iterations` s, a typo; we use `holdSec`) |
| `msgcolor` | `text\|r;g;b\|r;g;b` | ✅ |
| `conf` | `Section\|key:value\|...` | ⚠️ `DMDRenderer` `brightness` applied, `brightnesshours` stored; rest in phase 5 |
| `rebt` | – | ✅ `ESP.restart()` |
| `shutdwn` | – | ✅ blank panel + deep sleep (power cycle to restart) |
| `testFont` | `font.ttf` | ⚠️ sample text with built-in fonts (no TrueType on ESP32) |
| `soundeffet` | `text\|gif\|sound` | ⚠️ text part only; GIF in phase 3 |
| `sound` | `file` | ❌ no audio output on this board |
| `msgimg` | `text\|png` | ⚠️ text only; background PNG in phase 3 |
| `waiter`, `msgcarrou`, `time`, `testPattern` | | ⏳ phase 2 |
| `gif`, `gifText`, `gifPath`, `img`, `rand`, `demo`, `effet`, `excludeFolder`, `excludeFile` | | ⏳ phase 3 |
| `meteo`, `meteoPrevi`, `owmzc`, `fllcn`, `edfJoursTempo`, `perf` | | ⏳ phase 4 |
| `receipconf`, `rldconf` | | ⏳ phase 5 |

`sens`: `left`, `right`, `up`, `down`, `rotate`, `antirotate`, `flip`, `twirl`.

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
- Brightness: the original applies `brightnesshours[current hour]`; until the clock is ported (phase 2),
  `brightness` is applied directly.

## Phases

1. **Core engine**: non-blocking renderer, FIFO messages, text and movements, score, colours, brightness, reboot. *(this change)*
2. **Clock and attract mode**: NTP time, `time`, `waiter`, `scrollOrder` playlist, `brightnesshours`, text carousel.
3. **Media**: GIF/PNG from LittleFS, score and special-move animations, effects, exclusions, upload page.
4. **Online data**: OpenWeatherMap, EDF Tempo days.
5. **Settings**: full config schema, `receipconf`/`rldconf`, settings web page.
