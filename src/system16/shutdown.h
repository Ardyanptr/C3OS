#pragma once
#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>
#include <WiFi.h>

#include "app/Essential/Settings.h"
#include "component/service.h"
#include "emergency/eme_shutdown.h"

#include "system16/esp826.h"
#include "system16/state.h"

#include "config/config.h"

#include "esp_task_wdt.h"

extern void drawMenu();

inline void safe_shutdown() {
    sendCommand("avr32:blink");
    stopAllService();

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    btStop();

    sendCommand("avr32:blink");
    sendCommand("hold");
    sendCommand("avr32:deep-sleep-manual");
    sendCommand("avr32:power-down-all");

    display.setPowerSave(1);

    esp_sleep_config_gpio_isolate();

    pinMode(SDA_PIN, ANALOG);
    pinMode(SCL_PIN, ANALOG);
    pinMode(BUTTON_UP, INPUT);
    pinMode(BUTTON_DOWN, INPUT);
    pinMode(BUTTON_OK, INPUT);

    pinMode(BUTTON_ACTION, INPUT_PULLUP);

    delay(200);

    esp_deep_sleep_enable_gpio_wakeup(1ULL << BUTTON_ACTION, ESP_GPIO_WAKEUP_GPIO_LOW);
    gpio_deep_sleep_hold_en();

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