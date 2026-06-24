# CLAUDE.md — m5paper project notes

## Toolchain

- **arduino-cli**: `/home/virtuoso/Arduino/libraries/bin/arduino-cli` (v1.4.1)
- **Board FQBN**: `esp32:esp32:m5stack_paper`
- **esp32 core**: 3.3.7
- **Libraries**: M5Unified 0.2.17, M5GFX 0.2.23 (installed globally in `~/Arduino/libraries/`)
- **Device port**: `/dev/ttyUSB0` (Silicon Labs CP210x, user is in `dialout` group — no sudo needed)

## Library choice: M5Unified + M5GFX (not M5EPD)

We use **M5Unified** + **M5GFX** instead of the older **M5EPD** library. M5EPD 0.1.5
has at least 5 hard compile failures against arduino-esp32 3.x (GPIO register struct
changes, ADC enum strictness, JPEG callback signatures). M5EPD is also unmaintained.

M5Unified + M5GFX are actively maintained by M5Stack, support arduino-esp32 3.x, and
provide a cleaner API. Key differences from M5EPD:

| M5EPD (old)                        | M5Unified + M5GFX (current)                 |
|------------------------------------|---------------------------------------------|
| `#include <M5EPD.h>`               | `#include <M5Unified.h>`                    |
| `M5EPD_Canvas canvas(&M5.EPD)`     | `M5Canvas canvas(&M5.Display)`              |
| `canvas.createCanvas(w, h)`        | `canvas.createSprite(w, h)`                 |
| `canvas.fillCanvas(15)` (4-bit)    | `canvas.fillScreen(TFT_WHITE)`              |
| `canvas.pushCanvas(x, y, MODE)`    | `M5.Display.setEpdMode(mode); canvas.pushSprite(x, y)` |
| `UPDATE_MODE_GC16` / `UPDATE_MODE_DU` | `epd_quality` / `epd_fastest`           |
| `M5.RTC.setTime(&rtc_time_t)`      | `M5.Rtc.setDateTime(const tm*)`             |

EPD modes available via `setEpdMode()`:
- `epd_quality` — full refresh, clears ghosting (~450 ms equivalent of GC16)
- `epd_text` — optimised for text, mid-speed
- `epd_fast` — fast partial update
- `epd_fastest` — fastest partial update (~260 ms equivalent of DU)

The M5Paper default rotation in M5Unified is **portrait** (540 × 960). Call
`M5.Display.setRotation(1)` for landscape (960 × 540) before creating the canvas.

## WiFi credentials

WiFi credentials live in `m5paper_clock/secrets.h` (gitignored).
A template is at `m5paper_clock/secrets.h.template`.
Before flashing on a new machine:

```bash
cp m5paper_clock/secrets.h.template m5paper_clock/secrets.h
# edit secrets.h with real SSID / password
```

## Common commands

```bash
CLI=/home/virtuoso/Arduino/libraries/bin/arduino-cli

# Compile
$CLI compile --fqbn esp32:esp32:m5stack_paper m5paper_clock

# Upload
$CLI upload --fqbn esp32:esp32:m5stack_paper --port /dev/ttyUSB0 m5paper_clock

# Monitor serial
$CLI monitor --port /dev/ttyUSB0 --config 115200
```

## Note on the M5EPD pngle.c patch

A patch was applied to `~/Arduino/libraries/M5EPD/src/utility/pngle.c` to fix a
`<rom/miniz.h>` include that doesn't exist on arduino-esp32 3.x. This patch is
**not needed** for the current sketch (which uses M5Unified, not M5EPD), but is
noted here in case M5EPD is ever needed for another project on this machine.
The patch adds `#include <miniz.h>` as a fallback after the existing `<rom/miniz.h>`
check. Even with that fix, M5EPD has 4+ other 3.x incompatibilities — avoid it.
