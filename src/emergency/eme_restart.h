#pragma once
#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>
#include <WiFi.h>

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

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(2, 23, "Click OK to immediately restart");

    display.drawRBox(36, 30, 23, 9, 3);
    display.drawRFrame(63, 30, 33, 9, 3);

    display.setDrawColor(2);
    display.drawStr(43, 37, "OK");
    display.drawStr(68, 37, "CANCEL");

    display.sendBuffer();

    btnOK.attachClick([]() { eme_restart_run(); });
    btnOK.attachLongPressStart([]() { startService(); WiFi.mode(WIFI_STA); drawMenu(); });
}