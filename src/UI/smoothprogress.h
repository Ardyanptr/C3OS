#pragma once
#include <Arduino.h>

#include "config/config.h"
#include "floater.h"
#include "icons/icon.h"

inline void drawProgress(float value) {
    display.clearBuffer();

    int x0 = 34, y = 42;
    int x1 = x0 + (int)(59 * value);

    display.drawLine(x0, y, x1, y);
    display.drawXBM(60, 9, 11, 11, image_C3_bits);
    display.drawXBM(36, 55, 57, 9, image_wondrlan_bits);
    display.sendBuffer();
}

inline void smoothProgress(float from, float to, int durationMs) {
    unsigned long start = millis();
    while (millis() - start < durationMs) {
        float t = (millis() - start) / (float)durationMs;
        float eased = easeOut(t);
        float val = from + (to - from) * eased;
        drawProgress(val);
        delay(16);
    }

    drawProgress(to);
}