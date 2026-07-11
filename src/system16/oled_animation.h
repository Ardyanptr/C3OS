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