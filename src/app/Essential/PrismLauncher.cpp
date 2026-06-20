#include "PrismLauncher.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>

#include "ArduinoJson.hpp"
#include "ArduinoJson/Deserialization/DeserializationError.hpp"
#include "icons/icon.h"
#include "icons/prism_launcher.h"
#include "system16/esp826.h"

const char* current_os_version;
bool check_esp8266;
bool auto_sync;

bool isPrismLauncher_Running = true;

// External
void showPrismLauncherLogo() {
    display.clearBuffer();
    display.setBitmapMode(0);

    for (int i = 0; i < 25; i++) {
        display.clearBuffer();
        display.drawXBM(0, 0, 128, 64, epd_bitmap_allArray[i]);
        display.sendBuffer();
        delay(10);

        esp_task_wdt_reset();
    }
}

void drawWindowAppBackground(const char* title) {
    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);
    display.drawRFrame(4, 3, 121, 59, 3);

    display.setFont(u8g2_font_5x7_tr);
    display.drawStr(7, 13, title);

    display.drawLine(7, 15, 121, 15);
    display.sendBuffer();
}

// Problem
void showError(int type) {
    switch (type) {
        case 1:
            drawWindowAppBackground("Unexpected Error: 001");

            display.setFont(u8g2_font_4x6_tr);
            display.drawStr(7, 23, "Failed to load the PrismRC");
            display.drawStr(7, 30, "config file!");

            display.drawStr(7, 39, "Redownload the config to");
            display.drawStr(7, 45, "continue!");

            display.drawStr(7, 58, "Press OK to Exit.");
            display.sendBuffer();
            break;
        case 2:
            drawWindowAppBackground("Unexpected Error: 002");

            display.setFont(u8g2_font_4x6_tr);
            display.drawStr(7, 23, "JSON Parsing failed!");

            display.drawStr(7, 58, "Press OK to Exit.");
            display.sendBuffer();
            break;
    }
}

// Main
bool checkRC() {
    // Check the prismrc.JSON file
    File prismRC = LittleFS.open("/cfg/prismrc.json", "r");
    if (!prismRC) {
        showError(1);
        return false;
    }

    StaticJsonDocument<512> rc;
    DeserializationError rc_error = deserializeJson(rc, prismRC);

    if (rc_error) {
        showError(2);
        return false;
    }

    current_os_version = strdup(rc["current_os_version"] | "unknown");
    check_esp8266 = rc["check_esp8266"] | true;
    auto_sync = rc["auto_sync"] | true;

    Serial.println("[ PRISM ]: Successfully loading the config file!");
    prismRC.close();

    return true;
}

void runPrismLauncher() {
    isPrismLauncher_Running = true;

    display.clearBuffer();

    drawWindowAppBackground("Initializing...");

    display.setDrawColor(2);
    display.drawTriangle(51, 26, 61, 44, 42, 44);
    display.drawBox(54, 23, 20, 19);
    display.drawFilledEllipse(75, 38, 7, 7);
    display.drawLine(46, 20, 86, 47);

    display.setDrawColor(1);
    display.drawLine(7, 51, 120, 51);

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(7, 59, "Loading Config...");
    display.sendBuffer();

    if (!checkRC()) {
        isPrismLauncher_Running = false;
    }

    showPrismLauncherLogo();
    delay(100);

    esp_task_wdt_reset();

    display.clearBuffer();
    display.drawXBMP(6, 12, 32, 40, image_Updating_bits);

    display.setFont(u8g2_font_5x8_tr);
    display.drawStr(41, 19, "Preparing ESP8266");

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(41, 27, auto_sync ? "Auto-Sync was ON!" : "Auto-Sync was OFF!");
    display.sendBuffer();

    esp_task_wdt_reset();
    btnOK.attachClick([]() { isPrismLauncher_Running = false; });

    while (isPrismLauncher_Running) {
        yield();
        esp_task_wdt_reset();

        btnOK.tick();
    }
}