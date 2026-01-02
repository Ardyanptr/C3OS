#pragma once
#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>
#include <WiFi.h>

#include "component/service.h"
#include "system16/esp826.h"
#include "system16/state.h"

extern U8G2_SSD1315_128X64_NONAME_F_HW_I2C display;
extern OneButton btnUp;
extern OneButton btnDown;
extern OneButton btnOK;
extern OneButton btnAction;

extern void drawMenu();

inline void eme_shutdown_run() {
    boot_mode = BOOT_NORMAL;

    stopAllService();

    WiFi.disconnect(true);
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
    esp_deep_sleep_start();
}

inline void show_eme_shutdown() {
    stopAllService();
    WiFi.mode(WIFI_OFF);

    display.clearBuffer();

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(2, 27, "Click OK to immediately lost");

    display.drawRBox(36, 30, 23, 9, 3);
    display.drawRFrame(63, 30, 33, 9, 3);

    display.setDrawColor(2);
    display.drawStr(43, 37, "OK");
    display.drawStr(68, 37, "CANCEL");

    display.sendBuffer();

    btnOK.attachClick([]() { eme_shutdown_run(); });
    btnOK.attachClick([]() { startService(); WiFi.mode(WIFI_STA); drawMenu(); });
}