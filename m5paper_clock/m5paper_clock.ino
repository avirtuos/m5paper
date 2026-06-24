// m5paper_clock.ino
// ─────────────────────────────────────────────────────────────────────────────
// Displays "Hello World" + the current time (HH:MM) + date on the M5Paper's
// 4.7" e-ink display, refreshing every minute.
//
// Time source: WiFi + NTP.  The synced time is written into the onboard
// BM8563 RTC so the clock keeps ticking through WiFi drops; the RTC is
// re-synced to NTP once per hour.
//
// Prerequisites:
//   1. Install the M5EPD library (arduino-cli lib install M5EPD)
//   2. Copy secrets.h.template → secrets.h and fill in your WiFi credentials.
// ─────────────────────────────────────────────────────────────────────────────

#include <M5EPD.h>
#include <WiFi.h>
#include <time.h>
#include "secrets.h"   // <── you must create this from secrets.h.template

// ── Timezone (POSIX TZ string) ───────────────────────────────────────────────
// America/New_York.  Change for your location:
//   UTC:              "UTC0"
//   America/Chicago:  "CST6CDT,M3.2.0,M11.1.0"
//   America/Denver:   "MST7MDT,M3.2.0,M11.1.0"
//   America/Los_Angeles: "PST8PDT,M3.2.0,M11.1.0"
#define TZ_INFO     "EST5EDT,M3.2.0,M11.1.0"
#define NTP_SERVER1 "pool.ntp.org"
#define NTP_SERVER2 "time.nist.gov"

// ── Canvas / layout ──────────────────────────────────────────────────────────
#define CANVAS_W  960
#define CANVAS_H  540

// M5EPD canvas colors are 4-bit grayscale: 0 = black, 15 = white
#define C_BLACK  0
#define C_WHITE  15

// How often to do a full GC16 refresh (clears ghosting).
// Every minute is 1 tick, so 60 = once per hour.
#define FULL_REFRESH_EVERY  60

// ── Globals ──────────────────────────────────────────────────────────────────
M5EPD_Canvas canvas(&M5.EPD);

int lastMinute   = -1;
int lastNTPHour  = -1;
int tickCount    = 0;   // partial-update counter; triggers full refresh

// ── WiFi ─────────────────────────────────────────────────────────────────────
bool connectWiFi() {
    Serial.printf("[WiFi] Connecting to \"%s\"\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) {
        delay(500);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Connected, IP: %s\n",
                      WiFi.localIP().toString().c_str());
        return true;
    }
    Serial.println("[WiFi] Timed out — running on RTC only");
    return false;
}

// ── NTP sync → RTC ───────────────────────────────────────────────────────────
bool syncNTP() {
    if (WiFi.status() != WL_CONNECTED) {
        if (!connectWiFi()) return false;
    }

    configTzTime(TZ_INFO, NTP_SERVER1, NTP_SERVER2);
    Serial.print("[NTP] Waiting for sync");

    struct tm t;
    for (int i = 0; i < 20 && !getLocalTime(&t); i++) {
        delay(500);
        Serial.print('.');
    }
    Serial.println();

    if (!getLocalTime(&t)) {
        Serial.println("[NTP] Sync timed out");
        return false;
    }

    // Commit the synced time to the BM8563 RTC
    rtc_time_t rt;
    rt.hour = (int8_t)t.tm_hour;
    rt.min  = (int8_t)t.tm_min;
    rt.sec  = (int8_t)t.tm_sec;
    M5.RTC.setTime(&rt);

    rtc_date_t rd;
    rd.week = (int8_t)t.tm_wday;
    rd.mon  = (int8_t)(t.tm_mon + 1);
    rd.day  = (int8_t)t.tm_mday;
    rd.year = (int16_t)(t.tm_year + 1900);
    M5.RTC.setDate(&rd);

    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
    Serial.printf("[NTP] Synced: %s\n", buf);
    return true;
}

// ── Time source: system time, fallback to RTC ────────────────────────────────
bool getTimeNow(struct tm *t) {
    if (getLocalTime(t)) return true;

    // System time not set — read hardware RTC directly
    rtc_time_t rt;
    rtc_date_t rd;
    M5.RTC.getTime(&rt);
    M5.RTC.getDate(&rd);

    memset(t, 0, sizeof(struct tm));
    t->tm_hour = rt.hour;
    t->tm_min  = rt.min;
    t->tm_sec  = rt.sec;
    t->tm_wday = rd.week;
    t->tm_mday = rd.day;
    t->tm_mon  = rd.mon - 1;
    t->tm_year = rd.year - 1900;
    return (rd.year >= 2020);   // plausibility check
}

// ── Render and push the clock face ───────────────────────────────────────────
void drawClock() {
    struct tm t;
    if (!getTimeNow(&t)) {
        Serial.println("[draw] No valid time source — skipping");
        return;
    }

    char timeBuf[6];   // "HH:MM"
    char dateBuf[32];  // "Monday, June 23 2026"
    strftime(timeBuf, sizeof(timeBuf), "%H:%M", &t);
    strftime(dateBuf, sizeof(dateBuf), "%A, %B %d %Y", &t);

    // ── Build the frame ──────────────────────────────────────────────────────
    canvas.fillCanvas(C_WHITE);
    canvas.setTextColor(C_BLACK, C_WHITE);

    // "Hello World" — large, top-centered
    canvas.setTextSize(5);
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString("Hello World", CANVAS_W / 2, 48);

    // Horizontal rule beneath the header
    canvas.drawFastHLine(60, 152, CANVAS_W - 120, 3, C_BLACK);

    // Big clock — centered on screen
    canvas.setTextSize(9);
    canvas.setTextDatum(MC_DATUM);
    canvas.drawString(timeBuf, CANVAS_W / 2, 300);

    // Date line — bottom-centered
    canvas.setTextSize(3);
    canvas.setTextDatum(BC_DATUM);
    canvas.drawString(dateBuf, CANVAS_W / 2, 500);

    // ── Push to panel ────────────────────────────────────────────────────────
    // Full GC16 refresh every FULL_REFRESH_EVERY ticks to clear ghosting;
    // fast DU (monochrome, 260 ms) for all other updates.
    bool doFull = (tickCount == 0);
    canvas.pushCanvas(0, 0, doFull ? UPDATE_MODE_GC16 : UPDATE_MODE_DU);

    if (++tickCount >= FULL_REFRESH_EVERY) tickCount = 0;

    Serial.printf("[draw] %s  %s  (%s refresh)\n",
                  timeBuf, dateBuf, doFull ? "FULL" : "partial");
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    M5.begin();
    M5.EPD.SetRotation(90);   // landscape: 960 × 540
    M5.EPD.Clear(true);       // full clear (removes any ghosting from prior use)
    M5.RTC.begin();

    canvas.createCanvas(CANVAS_W, CANVAS_H);

    // Show a status message while we sort out WiFi + NTP
    canvas.fillCanvas(C_WHITE);
    canvas.setTextColor(C_BLACK, C_WHITE);
    canvas.setTextSize(3);
    canvas.setTextDatum(MC_DATUM);
    canvas.drawString("Connecting to WiFi...", CANVAS_W / 2, CANVAS_H / 2);
    canvas.pushCanvas(0, 0, UPDATE_MODE_GC16);

    connectWiFi();
    syncNTP();

    drawClock();
}

// ── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
    struct tm t;
    if (!getTimeNow(&t)) {
        delay(1000);
        return;
    }

    // Redraw whenever the minute changes
    if (t.tm_min != lastMinute) {
        lastMinute = t.tm_min;
        drawClock();

        // Re-sync NTP once per hour
        if (t.tm_hour != lastNTPHour) {
            lastNTPHour = t.tm_hour;
            syncNTP();
        }

        // Re-read time after draw + optional NTP (takes a few hundred ms)
        if (!getTimeNow(&t)) t.tm_sec = 0;
    }

    // Sleep until ~2 seconds before the next minute, then re-check
    int secsLeft = 58 - t.tm_sec;
    delay((secsLeft > 0 ? secsLeft : 1) * 1000L);
}
