#pragma once
#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>
#include <WiFi.h>

#include "UI/floater.h"
#include "app/Essential/Settings.h"
#include "component/service.h"
#include "emergency/eme_shutdown.h"
#include "icons/bg.h"
#include "system16/esp826.h"
#include "system16/state.h"

extern U8G2_SSD1315_128X64_NONAME_F_HW_I2C display;
extern OneButton btnUp;
extern OneButton btnDown;
extern OneButton btnOK;
extern OneButton btnAction;

extern void drawMenu();

inline void animateOLEDOff_Shutdown() {
    const int W = 128;
    const int H = 64;
    const int START_BAND = 24;
    const int DURATION = 400;

    unsigned long t0 = millis();

    while (true) {
        float t = (millis() - t0) / (float)DURATION;
        if (t > 1.0f) t = 1.0f;
        float e = easeOutCubic(t);

        int band = START_BAND - (int)(START_BAND * e);
        if (band < 1) band = 1;
        int y = (H - band) / 2;

        display.clearBuffer();

        display.drawBox(0, y, W, band);
        display.sendBuffer();

        if (t >= 1.0f) break;
        delay(12);
    }

    display.setPowerSave(1);
}

inline void safe_shutdown() {
    stopAllService();

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    btStop();
    delay(50);

    const char* shutdown_message = "Shutting Down...";
    const int W = 128;
    const int H = 64;

    for (int i = 0; i < 20; i++) {
        display.clearBuffer();
        display.setFont(u8g2_font_5x8_tr);

        int text_w = display.getStrWidth(shutdown_message);
        display.drawStr(64 - (text_w / 2), 32, shutdown_message);

        int current_contrast = 255 - (i * (255 / 20));
        if (current_contrast < 0) current_contrast = 0;
        display.setContrast(current_contrast);

        display.sendBuffer();
        delay(30);
    }

    animateOLEDOff_Shutdown();

    sendCommand("avr32:deep-sleep-manual");

    display.setContrast(0);
    display.setPowerSave(1);

    esp_sleep_config_gpio_isolate();
    pinMode(GPIO_NUM_3, INPUT_PULLUP);

    delay(200);

    esp_deep_sleep_enable_gpio_wakeup(1ULL << GPIO_NUM_3, ESP_GPIO_WAKEUP_GPIO_LOW);
    esp_deep_sleep_start();
}

inline void shutdown_now() {
    display.clearDisplay();

    display.setFontMode(1);
    display.setBitmapMode(1);

    display.drawXBM(0, 0, 127, 64, image_bg_bits);

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