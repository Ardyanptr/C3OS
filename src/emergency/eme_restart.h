#pragma once
#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>
#include <WiFi.h>

#include "component/service.h"
#include "icons/icon.h"
#include "system16/esp826.h"

extern U8G2_SSD1315_128X64_NONAME_F_HW_I2C display;
extern OneButton btnUp;
extern OneButton btnDown;
extern OneButton btnOK;
extern OneButton btnAction;

extern void drawMenu();

inline void eme_restart_run() {
    display.clearBuffer();
    display.sendBuffer();

    display.setFontMode(1);
    display.setBitmapMode(1);

    display.drawRFrame(1, 1, 72, 11, 3);
    display.drawXBM(4, 3, 7, 7, image_Hashmark_bits);
    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(14, 9, "Please Wait...");

    display.drawRFrame(1, 14, 110, 48, 3);
    display.drawStr(4, 32, "Emergency Restart!");
    display.sendBuffer();

    display.drawStr(8, 30, "- Turning Off Service...");
    display.sendBuffer();
    
    stopAllService();

    display.drawStr(8, 37, "- Unloading Components");
    display.sendBuffer();

    WiFi.mode(WIFI_OFF);
    btStop();

    display.drawStr(8, 44, "- Unloading ESP8266");
    display.sendBuffer();

    display.drawStr(4, 58, "Wait. May take a while!");
    display.sendBuffer();

    sendCommand("32:start");
    delay(200);
    sendCommand("avr32:force-restart");
    delay(2000);

    esp_restart();
}

inline void show_eme_restart() {
    stopAllService();
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