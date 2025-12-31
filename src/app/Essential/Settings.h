#pragma once

#include <Arduino.h>
#include <OneButton.h>
#include <SparkFun_External_EEPROM.h>
#include <U8g2lib.h>

// Extern dari main agar bisa akses display dan button
extern U8G2_SSD1315_128X64_NONAME_F_HW_I2C display;
extern OneButton btnUp;
extern OneButton btnDown;
extern OneButton btnOK;

extern void drawMenu();
extern void appHeartBeat();

class Settings {
   public:
    struct Data {
        uint32_t signature;
        bool bluetooth;
        bool wifi;
        uint8_t wifiPower;      // 0-100%
        uint8_t oledContrast;   // 0-255
        uint32_t sleepTimeout;  // ms
        uint8_t checksum;
    } __attribute__((packed));

    Settings();
    static Settings* instance;

    void begin();
    void run();
    void loadSettings();
    Data& get();

   private:
    ExternalEEPROM myMem;
    Data data;

    // UI State
    uint8_t cursor = 0;
    bool editing = false;
    bool isRunning = false;

    // Animation variables
    float animCursorY = 0;
    const uint8_t itemHeight = 12;

    // Input Flags
    volatile bool flagUp = false;
    volatile bool flagDown = false;
    volatile bool flagOK = false;
    volatile bool flagExit = false;

    void load();
    void save();
    void apply();
    void draw();
    void handleInput();

    // Callbacks internal
    static void onUp();
    static void onDown();
    static void onOK();
    static void onExit();
};