#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <OneButton.h>
#include <U8g2lib.h>

#include "config/config.h"

extern void drawMenu();
extern void appHeartBeat();

class Settings {
   public:
    struct Data {
        uint32_t signature;
        bool bluetooth;
        bool wifi;
        uint8_t wifiPower;
        uint8_t oledContrast;
        uint32_t sleepTimeout;
        uint8_t checksum;
    } __attribute__((packed));

    Settings();
    static Settings* instance;

    void begin();
    void run();
    void loadSettings();
    Data& get();

   private:
    Data data;
    const char* SETTINGS_PATH = "/settings.bin";

    uint8_t cursor = 0;
    bool editing = false;
    bool isRunning = false;

    float animCursorY = 0;
    const uint8_t itemHeight = 12;

    volatile bool flagUp = false;
    volatile bool flagDown = false;
    volatile bool flagOK = false;
    volatile bool flagExit = false;

    void load();
    void save();
    void apply();
    void draw();
    void handleInput();

    static void onUp();
    static void onDown();
    static void onOK();
    static void onExit();
};