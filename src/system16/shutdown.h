#pragma once
#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>
#include <WiFi.h>

#include "UI/floater.h"
#include "app/Essential/Settings.h"
#include "component/service.h"
#include "emergency/eme_shutdown.h"
#include "esp_task_wdt.h"
#include "system16/esp826.h"
#include "system16/state.h"

#include "config/config.h"

extern void drawMenu();

inline float easeOutQuart(float t) { return 1.0f - pow(1.0f - t, 4); }

enum class ShutdownStage { SAVING, DISCONNECTING, POWERING_OFF, DONE };

inline const char *getStageText(ShutdownStage stage) {
    switch (stage) {
    case ShutdownStage::SAVING:
        return "Saving";
    case ShutdownStage::DISCONNECTING:
        return "Disconnecting";
    case ShutdownStage::POWERING_OFF:
        return "Powering Off";
    case ShutdownStage::DONE:
        return "";
    }

    return "";
}

inline void drawProgressRing(int centerX, int centerY, int radius, float progress,
                             ShutdownStage stage, uint8_t opacity = 255) {
    const int segments = 12;
    const float gap = 0.15f;

    for (int i = 0; i < segments; i++) {
        float angle = (i / (float)segments) * TWO_PI - PI / 2;
        int x1 = centerX + cos(angle) * (radius - 2);
        int y1 = centerY + sin(angle) * (radius - 2);
        int x2 = centerX + cos(angle) * radius;
        int y2 = centerY + sin(angle) * radius;
        display.drawLine(x1, y1, x2, y2);
    }

    int activeSegments = (int)(segments * progress);
    display.setDrawColor(1);

    for (int i = 0; i < segments; i++) {
        float angle = (i / (float)segments) * TWO_PI - PI / 2;

        int x1 = centerX + cos(angle) * (radius - 3);
        int y1 = centerY + sin(angle) * (radius - 3);
        int x2 = centerX + cos(angle) * radius;
        int y2 = centerY + sin(angle) * radius;
        display.drawLine(x1, y1, x2, y2);
        display.drawLine(x1 + 1, y1, x2 + 1, y2);
    }

    if (progress > 0 && progress < 1) {
        float currentAngle = (progress * TWO_PI) - PI / 2;
        int dotX = centerX + cos(currentAngle) * radius;
        int dotY = centerY + sin(currentAngle) * radius;
        display.drawDisc(dotX, dotY, 2);
    }

    display.setFont(u8g2_font_5x8_tr);

    const char *text = getStageText(stage);
    int textW = display.getStrWidth(text);

    display.drawStr(centerX - textW / 2, centerY + radius + 12, text);
}

inline void animateOLEDOff_Shutdown() {
    const int W = 128;
    const int H = 64;
    const int DURATION = 600;

    unsigned long t0 = millis();

    while (true) {
        esp_task_wdt_reset();
        float t = (millis() - t0) / (float)DURATION;
        if (t > 1.0f) t = 1.0f;

        float e = easeInOutCubic(t);

        display.clearBuffer();

        // Scanline collapse effect: horizontal lines converging to center
        int activeHeight = H * (1.0f - e);
        int yStart = (H - activeHeight) / 2;

        // Draw only the middle band with decreasing height
        if (activeHeight > 0) {
            // Gradient fade within the band
            for (int y = 0; y < activeHeight; y++) {
                int actualY = yStart + y;
                int brightness = 255;
                if (y < 4) brightness = (y / 4.0f) * 255; // Fade top
                if (y > activeHeight - 4)
                    brightness = ((activeHeight - y) / 4.0f) * 255; // Fade bottom

                if (brightness > 128) {
                    display.drawLine(0, actualY, W, actualY);
                }
            }
        }

        // Add subtle noise/static effect in final phase
        if (t > 0.8f) {
            int noiseIntensity = (t - 0.8f) * 5 * 255; // Increasing noise
            // Random speckles (pseudo-random based on time)
            for (int i = 0; i < 10; i++) {
                int nx = (millis() + i * 13) % W;
                int ny = yStart + ((millis() + i * 7) % activeHeight);
                if (ny >= yStart && ny < yStart + activeHeight) {
                    display.drawPixel(nx, ny);
                }
            }
        }

        display.sendBuffer();

        if (t >= 1.0f) break;
        delay(16); // ~60fps
    }

    display.setPowerSave(1);
}

inline void safe_shutdown() {
    // Initial commands
    sendCommand("avr32:blink");
    stopAllService();

    // Disconnect services
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    btStop();

    const int CENTER_X = 64;
    const int CENTER_Y = 28;
    const int RADIUS = 14;
    const int STAGE_DURATION = 400; // ms per stage

    // Stage 1: Saving (0.0 - 0.33)
    unsigned long stageStart = millis();
    while (millis() - stageStart < STAGE_DURATION) {
        esp_task_wdt_reset();
        float t = (millis() - stageStart) / (float)STAGE_DURATION;

        display.clearBuffer();
        drawProgressRing(CENTER_X, CENTER_Y, RADIUS, t * 0.33f, ShutdownStage::SAVING);
        display.sendBuffer();
        delay(16);
    }

    // Stage 2: Disconnecting (0.33 - 0.66)
    stageStart = millis();
    while (millis() - stageStart < STAGE_DURATION) {
        esp_task_wdt_reset();
        float t = (millis() - stageStart) / (float)STAGE_DURATION;

        display.clearBuffer();
        drawProgressRing(CENTER_X, CENTER_Y, RADIUS, 0.33f + t * 0.33f,
                         ShutdownStage::DISCONNECTING);
        display.sendBuffer();
        delay(16);
    }

    // Stage 3: Power off (0.66 - 1.0)
    stageStart = millis();
    while (millis() - stageStart < STAGE_DURATION) {
        esp_task_wdt_reset();
        float t = (millis() - stageStart) / (float)STAGE_DURATION;

        display.clearBuffer();
        drawProgressRing(CENTER_X, CENTER_Y, RADIUS, 0.66f + t * 0.34f,
                         ShutdownStage::POWERING_OFF);
        display.sendBuffer();
        delay(16);
    }

    // Final "Goodbye" flash
    display.clearBuffer();
    display.setFont(u8g2_font_5x8_tr);
    const char *bye = "Goodbye";
    int w = display.getStrWidth(bye);
    display.drawStr(64 - w / 2, 32, bye);
    display.sendBuffer();
    delay(200);

    // Fade out text
    for (int i = 255; i >= 0; i -= 15) {
        display.setContrast(i);
        delay(20);
    }

    // Execute hardware commands
    sendCommand("avr32:blink");
    sendCommand("hold");
    sendCommand("avr32:deep-sleep-manual");
    sendCommand("avr32:power-down-all"); // Additional command for full peripheral power down

    // OLED power-down animation
    animateOLEDOff_Shutdown();
    display.setPowerSave(1);

    // CRITICAL: Isolate all GPIOs to prevent current leakage
    // ESP32-C3 has internal pull-ups that can drain current if not handled
    esp_sleep_config_gpio_isolate();
    
    // Set all used pins to high impedance/analog mode where possible
    pinMode(SDA_PIN, ANALOG);
    pinMode(SCL_PIN, ANALOG);
    pinMode(BUTTON_UP, INPUT);
    pinMode(BUTTON_DOWN, INPUT);
    pinMode(BUTTON_OK, INPUT);
    // Keep ACTION button as wakeup source
    pinMode(BUTTON_ACTION, INPUT_PULLUP);
    
    delay(200);

    // Configure Deep Sleep Wakeup
    esp_deep_sleep_enable_gpio_wakeup(1ULL << BUTTON_ACTION, ESP_GPIO_WAKEUP_GPIO_LOW);
    
    // Hold the state of critical pins if necessary (e.g., LDO EN pin if you have one)
    // gpio_hold_en((gpio_num_t)SOME_EN_PIN); 
    
    gpio_deep_sleep_hold_en();
    
    Serial.println("[SHUTDOWN] Entering deep sleep. Goodbye.");
    Serial.flush();
    
    delay(100);

    esp_deep_sleep_start();
}

inline void shutdown_now() {
    display.clearDisplay();

    display.setFontMode(1);
    display.setBitmapMode(1);

    display.drawRFrame(28, 19, 76, 30, 4);
    display.drawRBox(26, 17, 76, 30, 4);

    display.setDrawColor(2);

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(31, 26, "Proceed Shutdown?");

    display.drawRFrame(36, 33, 25, 9, 3);
    display.drawRFrame(65, 33, 25, 9, 3);

    display.drawStr(45, 40, "OK");
    display.drawStr(76, 40, "C");

    display.sendBuffer();

    btnOK.attachClick([]() { safe_shutdown(); });
    btnOK.attachDoubleClick([]() { drawMenu(); });
    btnOK.attachLongPressStart([]() { show_eme_shutdown(); });
}