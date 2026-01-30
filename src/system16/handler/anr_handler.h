#pragma once
#include <Arduino.h>

#include "LittleFS.h"
#include "config/config.h"
#include "config/var_declare.h"
#include "include/BLEAppLib.h"
#include "include/ComponentLib.h"
#include "include/CoreLib.h"
#include "include/EssentialAppLib.h"
#include "include/SystemCoreLib.h"
#include "include/UILib.h"
#include "include/WiFiAppLib.h"

extern int currentApp;
extern int anrApp;

extern uint32_t appLastBeat;
extern uint8_t anrSelect;

extern bool appRunning;
extern bool anrActive;
extern bool systemUIActive;

inline void killCurrentApp() {
    appRunning = false;
    currentApp = -1;
    anrActive = false;

    systemUIActive = true;
    drawMenu();
}

inline void drawANROverlay() {
    display.setDrawColor(0);
    display.drawBox(0, 0, 128, 64);
    display.setDrawColor(1);

    display.drawFrame(8, 10, 112, 44);

    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(16, 24, "Not Responding");
    display.drawStr(16, 36, appTable[anrApp].name);

    if (anrSelect == 0) display.drawBox(16, 44, 40, 12);
    if (anrSelect == 1) display.drawBox(72, 44, 40, 12);

    display.setDrawColor(anrSelect == 0 ? 0 : 1);
    display.drawStr(20, 53, "Wait");

    display.setDrawColor(anrSelect == 1 ? 0 : 1);
    display.drawStr(76, 53, "Force");

    display.setDrawColor(1);
    display.sendBuffer();
}

inline void anr_update_handler() {
    if (appRunning && !anrActive && !systemUIActive) {
        if (millis() - appLastBeat > 2000) {
            anrActive = true;
            anrApp = currentApp;
            anrSelect = 0;
        }
    }

    if (anrActive) {
        if (digitalRead(BUTTON_UP) == LOW || digitalRead(BUTTON_DOWN) == LOW) anrSelect ^= 1;

        if (digitalRead(BUTTON_OK) == LOW) {
            appLastBeat = millis();
            anrActive = false;
        }

        if (digitalRead(BUTTON_ACTION) == LOW) {
            killCurrentApp();
            anrActive = false;
        }

        return;
    }
}