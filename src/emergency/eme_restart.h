#pragma once
#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#include "component/service.h"
#include "icons/icon.h"
#include "system16/esp826.h"

#include "config/config.h"

extern void drawMenu();

inline void eme_restart_run() {
    esp_restart();
}

inline void show_eme_restart() {
    stopAllService();
    btStop();

    WiFi.mode(WIFI_OFF);

    display.clearBuffer();

    display.setFontMode(1);
    display.setBitmapMode(1);

    display.drawFrame(2, 29, 125, 6); // Outline
    
    display.sendBuffer();

    for (int i=0; i<125; i++) {
        display.drawBox(2, 29, 125, 6); // Inside
        display.sendBuffer();
        
        vTaskDelay(5 / portTICK_PERIOD_MS);
        esp_task_wdt_reset();
    }


    eme_restart_run(); // Just run no matter what
}