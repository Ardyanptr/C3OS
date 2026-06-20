#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include "config/config.h"
#include "icons/icon.h"
#include "wifiservice.h"

extern uint8_t getBatteryPercent();

namespace Liveness {

class StatusBar {
public:
    static void draw(U8G2& display) {
        display.setFont(u8g2_font_4x6_tr);
        display.setDrawColor(1);
        
        // 1. Heartbeat Pulse
        static float pulse = 0;
        pulse += 0.2f;
        int heartY = 2 + (int)(sin(pulse) * 1.5f);
        display.drawPixel(2, heartY + 2);
        display.drawPixel(3, heartY + 1);
        display.drawPixel(4, heartY + 2);
        display.drawPixel(3, heartY + 3);

        // 2. WiFi Status
        if (WiFi.status() == WL_CONNECTED) {
            int32_t rssi = WiFi.RSSI();
            int bars = 0;
            if (rssi > -50) bars = 4;
            else if (rssi > -60) bars = 3;
            else if (rssi > -70) bars = 2;
            else bars = 1;

            for (int i = 0; i < bars; i++) {
                display.drawVLine(10 + i * 2, 8 - (i + 1), i + 1);
            }
        } else {
            display.drawStr(10, 7, "x");
        }

        // 3. Battery Percentage (Estimated)
        uint8_t batt = getBatteryPercent();
        display.drawFrame(108, 2, 16, 6);
        display.drawBox(109, 3, (batt * 14) / 100, 4);
        display.drawPixel(124, 4);
        
        char battBuf[5];
        sprintf(battBuf, "%d%%", batt);
        display.drawStr(90, 7, battBuf);

        // 4. WiFi Speeds or OS Name
        if (WiFi.status() == WL_CONNECTED) {
            char speedBuf[32];
            // Using small representation for space efficiency
            String dStr = (g_wifiStats.downloadSpeed >= 100.0f) ? String((int)g_wifiStats.downloadSpeed) : String(g_wifiStats.downloadSpeed, 1);
            String uStr = (g_wifiStats.uploadSpeed >= 100.0f) ? String((int)g_wifiStats.uploadSpeed) : String(g_wifiStats.uploadSpeed, 1);
            
            snprintf(speedBuf, sizeof(speedBuf), "v%sK ^%sK", dStr.c_str(), uStr.c_str());
            display.drawStr(22, 7, speedBuf);
        } else {
            display.drawStr(40, 7, "C3OS");
        }

        // Separator line
        display.drawHLine(0, 9, 128);
    }
};

class Ambient {
public:
    struct Particle {
        float x, y, vx, vy;
    };
    static const int PARTICLE_COUNT = 15;
    static inline Particle particles[PARTICLE_COUNT];
    static inline bool initialized = false;

    static void updateAndDraw(U8G2& display) {
        if (!initialized) {
            for (int i = 0; i < PARTICLE_COUNT; i++) {
                particles[i] = { (float)random(128), (float)random(10, 64), (float)(random(10, 50) / 100.0f), (float)(random(-10, 10) / 100.0f) };
            }
            initialized = true;
        }

        display.setDrawColor(1);
        for (int i = 0; i < PARTICLE_COUNT; i++) {
            particles[i].x += particles[i].vx;
            particles[i].y += particles[i].vy;

            if (particles[i].x > 128) particles[i].x = 0;
            if (particles[i].y > 64) particles[i].y = 10;
            if (particles[i].y < 10) particles[i].y = 64;

            display.drawPixel((int)particles[i].x, (int)particles[i].y);
        }
    }
};

class Notifications {
private:
    static inline String currentMsg = "";
    static inline unsigned long startTime = 0;
    static inline bool active = false;

public:
    static void show(String msg) {
        currentMsg = msg;
        startTime = millis();
        active = true;
    }

    static void draw(U8G2& display) {
        if (!active) return;

        unsigned long elapsed = millis() - startTime;
        if (elapsed > 3000) {
            active = false;
            return;
        }

        int y = -12;
        if (elapsed < 500) y = -12 + (int)((elapsed / 500.0f) * 22);
        else if (elapsed > 2500) y = 10 - (int)(((elapsed - 2500) / 500.0f) * 22);
        else y = 10;

        display.setDrawColor(0);
        display.drawBox(0, y, 128, 12);
        display.setDrawColor(1);
        display.drawFrame(0, y, 128, 12);
        display.setFont(u8g2_font_4x6_tr);
        display.drawStr(5, y + 8, currentMsg.c_str());
    }
};

class Effects {
public:
    static void drawFocusPulse(U8G2& display, int y, int h) {
        static float pulse = 0;
        pulse += 0.15f;
        int alpha = (int)(127 + 128 * sin(pulse));
        // We can't do true alpha on 1-bit, but we can do a dotted pattern
        if (alpha > 180) {
            display.drawFrame(0, y, 128, h);
        }
    }
};

} // namespace Liveness
