#pragma once
#include <Arduino.h>

#include "esp_task_wdt.h"
#include "include/all_.h"

inline void batteryDead() {
    display.clearBuffer();
    display.drawXBM(52, 24, 24, 16, image_battery_10_bits);
    display.sendBuffer();
    delay(1500);

    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);
    display.drawXBM(50, 3, 29, 14, image_FaceNopower_bits);

    display.setFont(u8g2_font_5x7_tr);
    display.drawStr(14, 28, "Power Delivery Error");

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(3, 36, "Brownout was detected, consider");
    display.drawStr(26, 43, "shutdown the device!");

    display.setFont(u8g2_font_5x8_tr);
    display.drawStr(24, 53, "OK: Eme Shutdown");
    display.drawStr(32, 61, "OK2: Continue");

    display.sendBuffer();

    btnOK.attachClick([]() {
        eme_shutdown_run();
    });

    btnOK.attachDoubleClick([]() {
        display.clearBuffer();
        display.setFont(u8g2_font_5x7_tr);
        display.drawStr(12, 35, "Running on safe mode!");
        display.sendBuffer();

        vTaskDelay(1000 / portTICK_PERIOD_MS);

        btStop();
        WiFi.mode(WIFI_OFF);

        runBIOS_SAFE_MODE();
    });

    // halt the CPU
    while (true) {
        btnOK.tick();

        esp_task_wdt_reset();
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}