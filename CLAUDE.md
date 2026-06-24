# CLAUDE.md — m5paper project

## Git rules — read before every commit or push

- **Always ask the user for permission before `git commit` or `git push`.** Never commit
  or push autonomously, even for small or obvious changes.
- **`m5paper_clock/secrets.h` must never be committed.** It contains real WiFi credentials
  and is listed in `.gitignore`. Only `secrets.h.template` (placeholder values) belongs
  in the repo. Double-check before any broad `git add` command.

---

## M5Paper hardware

### MCU
- **ESP32-D0WDQ6-V3** — dual-core Xtensa LX6, 240 MHz
- 4 MB Flash (internal) + **16 MB external Flash**, **8 MB PSRAM**
- 2.4 GHz WiFi 802.11 b/g/n + Bluetooth 4.2 / BLE (built-in)

### E-ink display
- **4.7-inch** capacitive e-ink panel, **960 × 540 pixels**, 16-level grayscale
- Controller: **IT8951** (SPI)
- SPI pins: SCK=GPIO14, MOSI=GPIO12, MISO=GPIO13, CS=GPIO15, BUSY=GPIO27
- EPD power enable: GPIO23
- Default M5Unified orientation: **portrait** (540 wide × 960 tall)
- Use `M5.Display.setRotation(1)` for landscape (960 × 540)

#### EPD update modes (from IT8951 datasheet, via M5EPD_Driver.h)
| Mode | Time | Graytones | Use |
|------|------|-----------|-----|
| INIT | ~2000 ms | — | Full erase to white (startup only) |
| DU | ~260 ms | 2 (B/W) | Fast text/menu updates, low ghosting |
| GC16 | ~450 ms | 16 | High-quality images and text, very low ghosting |
| GL16 | ~450 ms | 16 | Sparse content on white background |
| DU4 | ~120 ms | 4 | Fast page-flip / animation |
| A2 | ~290 ms | 2 (B/W) | Animation, medium ghosting |

M5Unified/M5GFX equivalent modes (`setEpdMode()`):
- `epd_quality` ≈ GC16 — full refresh, clears ghosting
- `epd_text` — optimised for text, mid-speed
- `epd_fast` ≈ DU — fast partial update
- `epd_fastest` ≈ DU/A2 — fastest partial update

### RTC
- Chip: **BM8563** (I2C)
- Capabilities: get/set time (h/m/s) and date (year/month/day/weekday),
  alarm IRQ (by seconds, by time, or by date+time), software-triggered and
  RTC-wake shutdown

### Touch
- Chip: **GT911** (I2C, address `0x14`)
- Up to **2 simultaneous touch points** (x, y, id, size per finger)

### Environmental sensor
- Chip: **SHT30** (I2C) — temperature (°C/°F) and relative humidity

### Buttons
- LEFT: GPIO37, PUSH (centre): GPIO38, RIGHT: GPIO39

### Power / battery
- **1000 mAh Li-Po** battery with integrated management
- Main power latch: GPIO2 (hold high to stay on)
- External 5V port enable: GPIO5
- Battery voltage ADC: GPIO35
- `M5.Power.shutdown()` supports timed or RTC-alarm wake

### Expansion ports
| Port | Type | Pins |
|------|------|------|
| Port A | I2C (Grove) | SDA=GPIO32, SCL=GPIO25 |
| Port B | GPIO (Grove) | GPIO33, GPIO26 |
| Port C | UART (Grove) | TX=GPIO19, RX=GPIO18 |

### USB / serial
- Bridge: **Silicon Labs CP210x** (USB ID `10c4:ea60`)
- Linux device: `/dev/ttyUSB0`
- The kernel CP210x driver ships with Ubuntu/Debian — no manual install needed
- Serial monitor baud: **115200**

---

## Software ecosystem

### Libraries in use
| Library | Version | Purpose |
|---------|---------|---------|
| M5Unified | 0.2.17 | Board init, RTC, power management, display abstraction |
| M5GFX | 0.2.23 | Graphics / canvas (LGFX_Sprite-based), EPD modes |

Both are actively maintained by M5Stack and support **arduino-esp32 3.x**.

> **Do not use M5EPD** (the older M5Paper-specific library). M5EPD 0.1.5 has 5+
> hard compile failures against arduino-esp32 3.x (GPIO register struct changes,
> ADC enum type strictness, JPEG callback signatures). It is also unmaintained.

### Key M5Unified / M5GFX API for the M5Paper
```cpp
#include <M5Unified.h>

// Init
M5.begin();                               // auto-detects M5Paper board
M5.Display.setRotation(1);               // landscape 960×540
M5.Display.setEpdMode(epd_quality);      // set EPD refresh mode
M5.Display.clear();                      // full white clear

// Canvas (offscreen buffer → push to display)
M5Canvas canvas(&M5.Display);
canvas.createSprite(M5.Display.width(), M5.Display.height()); // PSRAM-backed
canvas.fillScreen(TFT_WHITE);
canvas.setTextColor(TFT_BLACK, TFT_WHITE);
canvas.setTextSize(5);
canvas.setTextDatum(top_center);         // from using namespace lgfx::textdatum
canvas.drawString("Hello", w/2, 48);
canvas.fillRect(60, 150, w-120, 3, TFT_BLACK);
canvas.pushSprite(0, 0);                 // uses current setEpdMode()

// RTC — set from struct tm (e.g. after NTP sync)
M5.Rtc.setDateTime(&tmStruct);          // const tm* overload
auto dt = M5.Rtc.getDateTime();         // returns rtc_datetime_t
// dt.date.year / .month / .date / .weekDay
// dt.time.hours / .minutes / .seconds

// WiFi + NTP (standard Arduino ESP32)
WiFi.begin(SSID, PASS);
configTzTime("EST5EDT,M3.2.0,M11.1.0", "pool.ntp.org");
struct tm t; getLocalTime(&t);
```

---

## Development prerequisites

### One-time system setup (fresh machine)

**1. arduino-cli**
```bash
# Download and install (Linux x64)
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
# or via package manager — see https://arduino.github.io/arduino-cli/latest/installation/

# Configure board manager URL for esp32
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

**2. esp32 board support**
```bash
arduino-cli core update-index
arduino-cli core install esp32:esp32          # installs latest 3.x
# or pin a version:
arduino-cli core install esp32:esp32@3.3.7
```

**3. Required libraries**
```bash
arduino-cli lib install "M5Unified" "M5GFX"
```

**4. USB serial permissions (Linux)**
```bash
sudo usermod -aG dialout $USER
# Log out and back in for the group change to take effect.
# Verify: groups | grep dialout
```
No additional CP210x driver install is needed — the `cp210x` kernel module ships
with all mainstream Linux distros.

**5. WiFi credentials file**
```bash
cp m5paper_clock/secrets.h.template m5paper_clock/secrets.h
# Edit secrets.h — fill in WIFI_SSID and WIFI_PASS
```

---

## Common commands

All commands assume arduino-cli is on `$PATH`. If it was installed to
`~/Arduino/libraries/bin/arduino-cli` (as on this machine), prefix accordingly or
add that directory to `$PATH`.

```bash
CLI=/home/virtuoso/Arduino/libraries/bin/arduino-cli   # this machine
FQBN=esp32:esp32:m5stack_paper
PORT=/dev/ttyUSB0

# Compile
$CLI compile --fqbn $FQBN m5paper_clock

# Upload (flash) to device
$CLI upload --fqbn $FQBN --port $PORT m5paper_clock

# Compile + upload in one step
$CLI compile --fqbn $FQBN --upload --port $PORT m5paper_clock

# Serial monitor (Ctrl-C to exit)
$CLI monitor --port $PORT --config 115200

# Update library index
$CLI lib update-index

# List installed libraries
$CLI lib list

# List installed cores
$CLI core list
```

### Troubleshooting uploads
- If upload hangs at "Connecting…", hold the M5Paper's power button for ~2 s to
  force a reset — the CP210x auto-reset usually handles this, but occasionally
  needs a nudge.
- Confirm the device is at `/dev/ttyUSB0`: `ls -l /dev/ttyUSB*`
- Confirm group membership: `groups | grep dialout`

---

## Timezone reference (POSIX TZ strings for `configTzTime`)

| Location | TZ string |
|----------|-----------|
| America/New_York | `EST5EDT,M3.2.0,M11.1.0` |
| America/Chicago | `CST6CDT,M3.2.0,M11.1.0` |
| America/Denver | `MST7MDT,M3.2.0,M11.1.0` |
| America/Los_Angeles | `PST8PDT,M3.2.0,M11.1.0` |
| UTC | `UTC0` |
| Europe/London | `GMT0BST,M3.5.0/1,M10.5.0` |
| Europe/Berlin | `CET-1CEST,M3.5.0,M10.5.0/3` |
