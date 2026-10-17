#pragma once
#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>
#include <WiFi.h>

#include "UI/dialog.h"
#include "app/Essential/Settings.h"
#include "component/service.h"
#include "emergency/eme_shutdown.h"
#include "system16/oled_animation.h"

#include "system16/esp826.h"
#include "system16/state.h"

#include "config/config.h"

#include "esp_task_wdt.h"

extern void drawMenu();

// Teardown + deep sleep. The device is not powered off: it sleeps and wakes on
// the ACTION button. Services are stopped, the radios are cut and the ESP32-C3
// goes into deep sleep with the ACTION pin armed as wake source.
inline void safe_shutdown() {
    sendCommand("avr32:blink");

    animateShutdown("Shutting down...");

    stopAllService();

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    btStop();

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
    DialogChoice choice = confirmDialog("Shutdown?", "Enters deep sleep - not a full power off",
                                        "ACTION wakes the device again");

    switch (choice) {
        case DialogChoice::CONFIRM:
            safe_shutdown();
            break;
        case DialogChoice::EMERGENCY:
            show_eme_shutdown();
            break;
        case DialogChoice::CANCEL:
        default:
            // ACTION is never re-bound by the dialog, the action menu just
            // resumes where it left off
            break;
    }
}
