#pragma once
#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#include "UI/dialog.h"
#include "component/service.h"
#include "icons/icon.h"
#include "system16/esp826.h"

#include "config/config.h"

extern void drawMenu();

inline void eme_restart_run() {
    esp_restart();
}

// Emergency restart: confirm first, then a progress bar while services wind
// down, then reboot.
inline void show_eme_restart() {
    DialogChoice choice = confirmDialog("Force restart?", "Reboots right now", "Nothing is saved");

    if (choice != DialogChoice::CONFIRM) return;

    stopAllService();
    btStop();

    WiFi.mode(WIFI_OFF);

    display.clearBuffer();

    display.setFontMode(1);
    display.setBitmapMode(1);

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(46, 24, "Rebooting...");

    const int BAR_X = 2, BAR_Y = 29, BAR_W = 125, BAR_H = 6;
    display.drawFrame(BAR_X, BAR_Y, BAR_W, BAR_H);
    display.sendBuffer();

    for (int i = 0; i < BAR_W; i++) {
        display.drawBox(BAR_X + i, BAR_Y + 1, 1, BAR_H - 2);
        if (i % 4 == 0) display.sendBuffer();

        vTaskDelay(5 / portTICK_PERIOD_MS);
        esp_task_wdt_reset();
    }

    display.sendBuffer();
    delay(120);

    eme_restart_run();
}
