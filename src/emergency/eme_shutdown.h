#pragma once
#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>
#include <WiFi.h>

#include "UI/dialog.h"
#include "component/service.h"
#include "system16/oled_animation.h"
#include "system16/esp826.h"
#include "system16/state.h"

#include "config/config.h"

extern void drawMenu();

// Cuts everything down and deep sleeps. Wakes on GPIO 3 (the ACTION button).
inline void eme_shutdown_run() {
    boot_mode = BOOT_NORMAL;

    stopAllService();

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(50);

    btStop();
    delay(50);

    sendCommand("avr32:deep-sleep-manual");

    fadeOutOLED();

    pinMode(GPIO_NUM_3, INPUT_PULLUP);

    esp_deep_sleep_enable_gpio_wakeup(1ULL << GPIO_NUM_3, ESP_GPIO_WAKEUP_GPIO_LOW);
    esp_deep_sleep_start();
}

// Emergency power-off, reached from the shutdown dialog with a long press.
// Services stay alive until the user actually confirms here.
inline void show_eme_shutdown() {
    DialogChoice choice = confirmDialog("Power off?", "Cuts power immediately", "Nothing is saved");

    if (choice == DialogChoice::CONFIRM) {
        eme_shutdown_run();
    }
}
