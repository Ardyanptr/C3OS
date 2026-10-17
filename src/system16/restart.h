#pragma once
#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>
#include <WiFi.h>

#include "UI/dialog.h"
#include "component/service.h"
#include "emergency/eme_restart.h"
#include "system16/state.h"

#include "config/config.h"

extern void drawMenu();

inline void safe_restart() {
    boot_mode = BOOT_NORMAL;

    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);
    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(40, 35, "Rebooting...");
    display.sendBuffer();

    display.setContrast(50); // Minimum contrast
    delay(50);

    stopAllService();
    WiFi.mode(WIFI_OFF);
    delay(50);
    esp_restart();
}

inline void restart_now() {
    DialogChoice choice = confirmDialog("Restart?", "Reboots the device", "Long-press OK: force restart");

    switch (choice) {
        case DialogChoice::CONFIRM:
            safe_restart();
            break;
        case DialogChoice::EMERGENCY:
            show_eme_restart();
            break;
        case DialogChoice::CANCEL:
        default:
            break;
    }
}
