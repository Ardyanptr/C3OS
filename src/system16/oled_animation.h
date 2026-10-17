#pragma once

#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>
#include <UI/floater.h>
#include <app/Essential/Settings.h>
#include <esp_task_wdt.h>

#include "config/config.h"
extern Settings setting;

extern void drawMenu();

inline void fadeInOLED() {
    display.setPowerSave(0);

    display.clearBuffer();
    display.sendBuffer();

    int step = 6;
    for (int c = 0; c <= Settings::instance->get().oledContrast; c += step) {
        display.setContrast(c);
        delay(2);
    }

    setting.loadSettings();
}

inline void fadeOutOLED() {
    int step = 6;
    for (int c = 180; c >= 0; c -= step) {
        display.setContrast(c);
        delay(2);
    }

    setting.loadSettings();
    display.setPowerSave(1);
}

inline void animateOLEDOff() {
    display.setPowerSave(0);
    const int W = 128;
    const int H = 64;
    const int START_BAND = 24;
    const int DURATION = 400;

    unsigned long t0 = millis();

    while (true) {
        esp_task_wdt_reset();
        float t = (millis() - t0) / (float)DURATION;
        if (t > 1.0f) t = 1.0f;
        float e = easeOutCubic(t);

        int band = START_BAND - (int)(START_BAND * e);
        if (band < 1) band = 1;
        int y = (H - band) / 2;

        display.clearBuffer();

        display.drawBox(0, y, W, band);
        display.sendBuffer();

        int c = 180 - (int)(e * 160);
        if (c < 20) c = 20;
        display.setContrast(c);

        if (t >= 1.0f) break;
        delay(12);
    }

    for (int c = 20; c >= 0; c -= 4) {
        display.setContrast(c);
        delay(8);
    }

    display.setPowerSave(1);
}

// Shutdown sequence: a status line with a progress bar (so the teardown reads
// as work actually happening), followed by an iris close and a fade to black.
inline void animateShutdown(const char* label) {
    const int W = 128;
    const int H = 64;

    display.setPowerSave(0);
    display.setFontMode(1);
    display.setBitmapMode(1);
    display.setDrawColor(1);
    display.setContrast(Settings::instance->get().oledContrast);

    const int BAR_W = 84, BAR_H = 5;
    const int BAR_X = (W - BAR_W) / 2, BAR_Y = 42;

    // Phase 1: label + progress bar
    const unsigned long PHASE_PROGRESS_MS = 500;
    unsigned long t0 = millis();
    while (true) {
        esp_task_wdt_reset();
        float t = (millis() - t0) / (float)PHASE_PROGRESS_MS;
        if (t > 1.0f) t = 1.0f;
        float e = easeOut(t);

        display.clearBuffer();

        display.setFont(u8g2_font_6x10_tr);
        display.drawStr((W - display.getStrWidth(label)) / 2, 26, label);

        display.drawRFrame(BAR_X, BAR_Y, BAR_W, BAR_H, 2);
        int fill = (int)((BAR_W - 4) * e);
        if (fill > 0) {
            display.drawBox(BAR_X + 2, BAR_Y + 2, fill, BAR_H - 4);
        }

        display.sendBuffer();

        if (t >= 1.0f) break;
        delay(10);
    }

    // Phase 2: iris close + contrast fade to black
    const unsigned long PHASE_IRIS_MS = 400;
    const int contrast = Settings::instance->get().oledContrast;
    t0 = millis();
    while (true) {
        esp_task_wdt_reset();
        float t = (millis() - t0) / (float)PHASE_IRIS_MS;
        if (t > 1.0f) t = 1.0f;
        float e = easeOutCubic(t);

        int inset = (int)((H / 2) * e);
        int h = H - 2 * inset;
        if (h < 1) h = 1;

        display.clearBuffer();
        display.drawFrame(0, inset, W, h);
        display.sendBuffer();

        int c = contrast - (int)(contrast * e);
        if (c < 0) c = 0;
        display.setContrast(c);

        if (t >= 1.0f) break;
        delay(10);
    }

    display.clearBuffer();
    display.sendBuffer();
    display.setPowerSave(1);
}