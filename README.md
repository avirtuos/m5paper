# m5paper

Arduino sketches for the [M5Stack M5Paper](https://docs.m5stack.com/en/core/m5paper) e-ink device.

---

## m5paper_clock

Displays **"Hello World"**, the current time (`HH:MM`), and the full date on the M5Paper's 4.7" 960×540 e-ink screen. The display updates **every minute**.

### How it works

- On boot, connects to WiFi and syncs exact time from NTP (`pool.ntp.org`).
- The synced time is written into the onboard **BM8563 RTC** so the clock keeps running through WiFi outages.
- NTP is re-synced once per hour to prevent drift.
- Each minute the display does a fast **DU partial update** (260 ms, no flash).
- Every 60 minutes it does a full **GC16 refresh** (450 ms) to clear e-ink ghosting.

### Setup

**1. Install the M5EPD library**

```bash
arduino-cli lib install M5EPD
```

**2. Add your WiFi credentials**

```bash
cp m5paper_clock/secrets.h.template m5paper_clock/secrets.h
# Edit secrets.h and fill in WIFI_SSID and WIFI_PASS
```

`secrets.h` is git-ignored — your password never gets committed.

**3. (Optional) Change the timezone**

Edit the `TZ_INFO` define at the top of `m5paper_clock.ino`. Defaults to `America/New_York`.

**4. Compile**

```bash
arduino-cli compile --fqbn esp32:esp32:m5stack_paper m5paper_clock
```

**5. Upload**

```bash
arduino-cli upload --fqbn esp32:esp32:m5stack_paper --port /dev/ttyUSB0 m5paper_clock
```

**6. Monitor serial output**

```bash
arduino-cli monitor --port /dev/ttyUSB0 --config 115200
```

You should see WiFi connect, NTP sync, and a `[draw]` line each minute.

### Requirements

| Component | Version |
|-----------|---------|
| arduino-cli | ≥ 1.0 |
| esp32 Arduino core | 3.x (tested on 3.3.7) |
| M5EPD library | 0.1.5 |
