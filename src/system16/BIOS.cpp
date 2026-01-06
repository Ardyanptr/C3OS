#include "BIOS.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <system16/boot/boot.h>

bool upFlag_BIOS = false;
bool downFlag_BIOS = false;
bool okFlag_BIOS = false;
bool actionFlag_BIOS = false;
bool okLong_BIOS = false;

void drawFan(int x, int y, int frame) {
    display.drawCircle(x, y, 6);
    float angle = frame * 0.4;
    for (int i = 0; i < 3; i++) {
        float a = angle + (i * 2.094);
        display.drawLine(x, y, x + cos(a) * 5, y + sin(a) * 5);
    }
}

void runBIOS_SAFE_MODE() {
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawFrame(10, 20, 108, 24);
    display.drawStr(15, 36, "MODE: SAFE_BOOT");
    display.sendBuffer();
    delay(1000);
    boot_mode = BOOT_SAFE;
    esp_sleep_enable_timer_wakeup(1ULL * 1000000);
    esp_deep_sleep_start();
}

void runBIOS() {
    AT24C256 eeprom(0x50);
    eeprom.begin();

    for (int i = 0; i < 10; i++) {
        display.clearBuffer();
        display.drawBox(0, 64 - (i * 6.4), 128, 2);
        display.setFont(u8g2_font_4x6_tr);
        display.drawStr(2, 60, "INITIALIZING BIOS...");
        display.sendBuffer();
    }

    const char* items[] = {
        "BRIGHTNESS", "SLEEP TIME", "I2C SCAN",
        "EEPROM TEST", "SYS INFO", "KILL SWITCH",
        "SAFE MODE", "REBOOT", "EXIT"};

    const uint8_t itemCount = sizeof(items) / sizeof(items[0]);
    int sel = 0;
    int fanFrame = 0;
    int brightness = 120;

    auto drawOSInterface = [&](int selected) {
        display.clearBuffer();

        display.drawBox(0, 0, 128, 9);
        display.setDrawColor(0);
        display.setFont(u8g2_font_5x8_tf);
        display.drawStr(4, 7, "C3OS SETUP UTILITY");
        drawFan(120, 4, fanFrame++);
        display.setDrawColor(1);

        for (int i = 10; i < 64; i += 4) display.drawPixel(85, i);

        for (int i = 0; i < 5; i++) {
            int idx = selected - 2 + i;
            if (idx >= 0 && idx < itemCount) {
                int yPos = 24 + (i * 9);
                if (idx == selected) {
                    display.drawBox(0, yPos - 7, 82, 9);
                    display.drawBox(82, yPos - 7, 2, 9);
                    display.setDrawColor(0);
                    display.drawStr(6, yPos, items[idx]);
                    display.setDrawColor(1);
                } else {
                    display.drawStr(4, yPos, items[idx]);
                }
            }
        }

        display.setFont(u8g2_font_4x6_tr);
        display.drawStr(89, 20, "[ INFO ]");

        if (selected == 0) {
            display.drawStr(89, 32, "LEVEL:");
            char bBuf[8];
            snprintf(bBuf, sizeof(bBuf), "%d", brightness);
            display.drawStr(89, 40, bBuf);
            display.drawFrame(89, 44, 35, 5);
            display.drawBox(91, 46, map(brightness, 0, 255, 0, 31), 1);
        } else if (selected == 4) {
            display.drawStr(89, 32, "ARCH:");
            display.drawStr(89, 40, "RISC-V");
            display.drawStr(89, 50, "C3-MINI");
        } else {
            display.drawStr(89, 32, "READY");
        }
        display.sendBuffer();
    };

    btnUp.attachClick([]() { upFlag_BIOS = true; });
    btnDown.attachClick([]() { downFlag_BIOS = true; });
    btnOK.attachClick([]() { okFlag_BIOS = true; });
    btnAction.attachClick([]() { actionFlag_BIOS = true; });
    btnOK.attachLongPressStart([]() { okLong_BIOS = true; });

    bool exitBIOS = false;
    while (!exitBIOS) {
        if (upFlag_BIOS) {
            sel = (sel - 1 + itemCount) % itemCount;
            upFlag_BIOS = false;
        }
        if (downFlag_BIOS) {
            sel = (sel + 1) % itemCount;
            downFlag_BIOS = false;
        }
        if (okLong_BIOS || actionFlag_BIOS) {
            exitBIOS = true;
            break;
        }

        if (okFlag_BIOS) {
            okFlag_BIOS = false;

            if (sel == 6) runBIOS_SAFE_MODE();
            if (sel == 7) {
                display.clearBuffer();
                display.setFont(u8g2_font_6x10_tf);
                display.drawStr(20, 35, "REBOOTING...");
                display.sendBuffer();
                delay(800);
                esp_restart();
            }
            if (sel == 8) exitBIOS = true;
        }

        btnUp.tick();
        btnDown.tick();
        btnOK.tick();
        btnAction.tick();
        drawOSInterface(sel);
        delay(15);
    }

    btnUp.attachClick(nullptr);
    btnDown.attachClick(nullptr);
    btnOK.attachClick(nullptr);
    btnAction.attachClick(nullptr);

    boot();
}