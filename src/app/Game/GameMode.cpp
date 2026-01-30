#include "GameMode.h"

#include <Arduino.h>

void runGameMode() {
    if (Settings::instance->get().gameMode) {
        setCpuFrequencyMhz(160);

        WiFi.mode(WIFI_OFF);
        btStop();

        display.clearBuffer();

        display.setFontMode(1);
        display.setBitmapMode(1);

        display.drawXBM(4, 28, 11, 8, image_GameMode_bits);
        display.drawRFrame(2, 26, 15, 13, 3);

        display.sendBuffer();
        delay(500);
        return;
    } else {
        setCpuFrequencyMhz(80);
        return;
    }
}