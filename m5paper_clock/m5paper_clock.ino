// m5paper_clock.ino
// ─────────────────────────────────────────────────────────────────────────────
// Displays "Hello World" + the current time (HH:MM) + date on the M5Paper's
// 4.7" e-ink display, refreshing every minute.
//
// Uses WiFi + NTP for accurate time. The synced time is written to the
// onboard BM8563 RTC via M5Unified so the clock keeps ticking through
// WiFi drops. NTP re-syncs once per hour.
//
// Libraries: M5Unified 0.2.x + M5GFX 0.2.x (arduino-esp32 3.x compatible)
//
// Prerequisites:
//   1. arduino-cli lib install M5Unified M5GFX
//   2. Copy secrets.h.template → secrets.h and fill in your WiFi credentials.
// ─────────────────────────────────────────────────────────────────────────────

#include <M5Unified.h>
#include <WiFi.h>
#include <time.h>
#include "secrets.h"   // <── create this from secrets.h.template

// ── Timezone (POSIX TZ string) ────────────────────────────────────────────────
// America/New_York. Change for your location:
//   UTC:                 "UTC0"
//   America/Chicago:     "CST6CDT,M3.2.0,M11.1.0"
//   America/Denver:      "MST7MDT,M3.2.0,M11.1.0"
//   America/Los_Angeles: "PST8PDT,M3.2.0,M11.1.0"
#define TZ_INFO     "EST5EDT,M3.2.0,M11.1.0"
#define NTP_SERVER1 "pool.ntp.org"
#define NTP_SERVER2 "time.nist.gov"

// Perform a full ghosting-clearing refresh every N minute ticks (60 = hourly).
#define FULL_REFRESH_EVERY 60

// ── Globals ───────────────────────────────────────────────────────────────────
M5Canvas canvas(&M5.Display);

int lastMinute  = -1;
int lastNTPHour = -1;
int tickCount   = 0;

// ── WiFi ──────────────────────────────────────────────────────────────────────
bool connectWiFi() {
    Serial.printf("[WiFi] Connecting to \"%s\"\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) {
        delay(500);
        Serial.print('.');
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }
    Serial.println("[WiFi] Timed out — running on RTC only");
    return false;
}

// ── NTP sync → RTC ────────────────────────────────────────────────────────────
bool syncNTP() {
    if (WiFi.status() != WL_CONNECTED) {
        if (!connectWiFi()) return false;
    }
    configTzTime(TZ_INFO, NTP_SERVER1, NTP_SERVER2);
    Serial.print("[NTP] Waiting");
    struct tm t;
    for (int i = 0; i < 20 && !getLocalTime(&t); i++) {
        delay(500);
        Serial.print('.');
    }
    Serial.println();
    if (!getLocalTime(&t)) {
        Serial.println("[NTP] Timed out");
        return false;
    }
    // Persist synced time to the BM8563 RTC so it survives WiFi drops.
    // M5Unified's setDateTime(const tm*) handles the struct conversion.
    M5.Rtc.setDateTime(&t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
    Serial.printf("[NTP] Synced: %s\n", buf);
    return true;
}

// ── Time source: system clock, fallback to RTC ────────────────────────────────
bool getTimeNow(struct tm *t) {
    if (getLocalTime(t)) return true;

    auto dt = M5.Rtc.getDateTime();
    if (dt.date.year < 2020) return false;

    memset(t, 0, sizeof(struct tm));
    t->tm_hour = dt.time.hours;
    t->tm_min  = dt.time.minutes;
    t->tm_sec  = dt.time.seconds;
    t->tm_mday = dt.date.date;
    t->tm_mon  = dt.date.month - 1;
    t->tm_year = dt.date.year - 1900;
    t->tm_wday = dt.date.weekDay;
    return true;
}

// ── Render and push the clock face ────────────────────────────────────────────
void drawClock() {
    struct tm t;
    if (!getTimeNow(&t)) {
        Serial.println("[draw] No valid time — skipping");
        return;
    }

    char timeBuf[6];   // "HH:MM"
    char dateBuf[32];  // "Monday, June 23 2026"
    strftime(timeBuf, sizeof(timeBuf), "%H:%M", &t);
    strftime(dateBuf, sizeof(dateBuf), "%A, %B %d %Y", &t);

    int w = canvas.width();
    int h = canvas.height();

    canvas.fillScreen(TFT_WHITE);
    canvas.setTextColor(TFT_BLACK, TFT_WHITE);

    // "Hello World" — large, top-centred
    canvas.setTextSize(5);
    canvas.setTextDatum(top_center);
    canvas.drawString("Hello World", w / 2, 48);

    // Horizontal rule beneath the header
    canvas.fillRect(60, 152, w - 120, 3, TFT_BLACK);

    // Big HH:MM clock
    canvas.setTextSize(9);
    canvas.setTextDatum(middle_center);
    canvas.drawString(timeBuf, w / 2, h / 2);

    // Date line at bottom
    canvas.setTextSize(3);
    canvas.setTextDatum(bottom_center);
    canvas.drawString(dateBuf, w / 2, h - 40);

    // Push — full quality refresh periodically to clear ghosting;
    // fastest partial update otherwise.
    bool doFull = (tickCount == 0);
    M5.Display.setEpdMode(doFull ? epd_quality : epd_fastest);
    canvas.pushSprite(0, 0);

    if (++tickCount >= FULL_REFRESH_EVERY) tickCount = 0;
    Serial.printf("[draw] %s  %s  (%s)\n", timeBuf, dateBuf, doFull ? "FULL" : "fast");
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    auto cfg = M5.config();
    cfg.internal_rtc = true;
    M5.begin(cfg);

    // Landscape: 960 × 540 (M5Paper default rotation is portrait)
    M5.Display.setRotation(1);
    M5.Display.setEpdMode(epd_quality);
    M5.Display.clear();

    canvas.createSprite(M5.Display.width(), M5.Display.height());

    // Show a splash screen while WiFi / NTP settle
    canvas.fillScreen(TFT_WHITE);
    canvas.setTextColor(TFT_BLACK, TFT_WHITE);
    canvas.setTextSize(3);
    canvas.setTextDatum(middle_center);
    canvas.drawString("Connecting to WiFi...", canvas.width() / 2, canvas.height() / 2);
    M5.Display.setEpdMode(epd_quality);
    canvas.pushSprite(0, 0);

    connectWiFi();
    syncNTP();
    drawClock();
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
    struct tm t;
    if (!getTimeNow(&t)) { delay(1000); return; }

    if (t.tm_min != lastMinute) {
        lastMinute = t.tm_min;
        drawClock();

        // Re-sync NTP once per hour
        if (t.tm_hour != lastNTPHour) {
            lastNTPHour = t.tm_hour;
            syncNTP();
        }

        // Re-read after draw + optional NTP (takes a few hundred ms)
        if (!getTimeNow(&t)) t.tm_sec = 0;
    }

    // Sleep until ~2 s before the next minute, then re-check
    int secsLeft = 58 - t.tm_sec;
    delay((secsLeft > 0 ? secsLeft : 1) * 1000L);
}
