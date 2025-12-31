#pragma once
#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>
#include <WiFi.h>

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

void safe_shutdown() {
    stopAllService();

    WiFi.mode(WIFI_OFF);
    delay(50);

    btStop();
    delay(50);

    sendCommand("avr32:deep-sleep-manual");
    display.setContrast(0);
    display.setPowerSave(1);
    delay(200);

    pinMode(GPIO_NUM_3, INPUT_PULLUP);

    esp_deep_sleep_enable_gpio_wakeup(1ULL << GPIO_NUM_3, ESP_GPIO_WAKEUP_GPIO_LOW);

    delay(100);
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