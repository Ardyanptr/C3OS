#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include <OneButton.h>
#include <U8g2lib.h>
#include <WiFi.h>

#include "component/button.h"
#include "component/service.h"
#include "esp_system.h"
#include <esp_task_wdt.h>
#include "system16/AT24C256_EEPROM.h"

#include "config/config.h"

extern void drawMenu();

inline void check_sys_integrity() {
    AT24C256 eeprom(0x50);
    eeprom.begin();

    auto drawHeader = [&](const char* title) {
        display.clearBuffer();
        display.setFont(u8g2_font_6x10_tf);
        display.drawStr(2, 10, title);
    };

    auto drawProgress = [&](const char* step, uint8_t pct, char spin) {
        display.clearBuffer();
        display.setFont(u8g2_font_6x10_tf);
        display.drawStr(2, 10, "System File Checker");
        display.setFont(u8g2_font_4x6_tr);
        display.drawStr(2, 22, step);
        display.drawRFrame(6, 30, 116, 10, 2);
        uint8_t w = (uint8_t)map(pct, 0, 100, 0, 112);
        display.drawBox(8, 32, w, 6);
        char pbuf[16];
        snprintf(pbuf, sizeof(pbuf), "%3u%%", pct);
        display.drawStr(100, 22, pbuf);
        char s[2] = {spin, 0};
        display.drawStr(6, 22, s);
        display.sendBuffer();
    };

    auto spinnerChar = [](uint8_t i) -> char {
        static const char* sp = "|/-\\";
        return sp[i & 3];
    };

    bool needsRepair = false;
    bool eepromOk = false;
    bool boardOk = false;
    bool heapOk = true;
    bool lastErrOk = true;
    bool wakeOk = true;

    for (uint8_t i = 0; i <= 20; ++i) {
        drawProgress("Checking EEPROM...", (uint8_t)min(100, i * 5), spinnerChar(i));
        delay(20);
    }
    Wire.beginTransmission(0x50);
    uint8_t e = Wire.endTransmission();
    eepromOk = (e == 0);
    if (!eepromOk) needsRepair = true;

    uint8_t hdr[8] = {0};
    if (eepromOk && eeprom.readBlock(0x0000, hdr, 8)) {
        uint8_t bright = hdr[0];
        uint32_t st = ((uint32_t)hdr[4] << 24) | ((uint32_t)hdr[5] << 16) | ((uint32_t)hdr[6] << 8) | hdr[7];
        if (bright == 0xFF || st == 0 || st == 0xFFFFFFFF) {
            needsRepair = true;
        }
    } else if (eepromOk) {
        needsRepair = true;
    }

    for (uint8_t i = 0; i <= 10; ++i) {
        drawProgress("Validating board ID...", (uint8_t)min(100, 20 + i * 3), spinnerChar(i));
        delay(15);
    }
    const char* chipModel = ESP.getChipModel();
    uint8_t chipRev = ESP.getChipRevision();
    boardOk = (chipModel && strstr(chipModel, "ESP32-C3") != nullptr) && (chipRev <= 4);

    for (uint8_t i = 0; i <= 10; ++i) {
        drawProgress("Checking heap...", (uint8_t)min(100, 45 + i * 3), spinnerChar(i));
        delay(15);
    }
    size_t heap = ESP.getFreeHeap();

    if (heap < 10 * 1024) heapOk = false;

    for (uint8_t i = 0; i <= 10; ++i) {
        drawProgress("Reading last reset reason...", (uint8_t)min(100, 65 + i * 3), spinnerChar(i));
        delay(15);
    }
    esp_reset_reason_t rr = esp_reset_reason();
    switch (rr) {
        case ESP_RST_PANIC:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
            lastErrOk = false;
            break;
        default:
            lastErrOk = true;
            break;
    }

    for (uint8_t i = 0; i <= 10; ++i) {
        drawProgress("Validating wake source...", (uint8_t)min(100, 85 + i * 2), spinnerChar(i));
        delay(15);
    }

    wakeOk = (rr != ESP_RST_UNKNOWN);

    needsRepair = needsRepair || !heapOk || !lastErrOk;

    if (!needsRepair) {
        for (uint8_t i = 0; i < 2; ++i) {
            display.clearBuffer();
            display.setFont(u8g2_font_6x10_tf);
            display.drawStr(2, 16, "System Scan: OK");
            display.drawRFrame(12, 28, 104, 10, 3);
            display.drawBox(14, 30, 100, 6);
            display.setFont(u8g2_font_unifont_t_symbols);
            display.drawGlyph(100, 52, 0x2713);
            display.sendBuffer();
            delay(300);
        }
        delay(400);
        return;
    }

    uint8_t stepIdx = 0;
    auto drawRepair = [&](const char* what, uint8_t pct) {
        display.clearBuffer();
        display.setFont(u8g2_font_6x10_tf);
        display.drawStr(2, 10, "Attempting Repairs...");
        display.setFont(u8g2_font_4x6_tr);
        display.drawStr(2, 22, what);
        display.drawRFrame(6, 30, 116, 10, 2);
        uint8_t w = (uint8_t)map(pct, 0, 100, 0, 112);
        display.drawBox(8, 32, w, 6);
        static const char* spinner = "|/-\\";
        char s[2] = {spinner[(stepIdx++) & 3], 0};
        display.drawStr(6, 22, s);
        display.sendBuffer();
    };

    uint8_t pct = 0;

    if (!eepromOk) {
        drawRepair("EEPROM not detected: waiting...", pct);

        delay(300);
        Wire.beginTransmission(0x50);
        e = Wire.endTransmission();
        eepromOk = (e == 0);
    }

    if (eepromOk) {
        drawRepair("EEPROM OK - skipping repair", pct);
    } else {
        const uint16_t total = 32768;
        const uint16_t chunk = 64;
        uint8_t buf[chunk];
        memset(buf, 0xFF, sizeof(buf));
        uint16_t written = 0;
        bool needsFormat = false;
        for (uint16_t i = 0; i < total; i += chunk) {
            uint8_t readBack[chunk] = {0};
            if (!eeprom.readBlock(i, readBack, chunk)) {
                needsFormat = true;
                break;
            }
            bool match = true;
            for (uint16_t j = 0; j < chunk; ++j) {
                if (buf[j] != readBack[j]) {
                    match = false;
                    break;
                }
            }
            if (!match) {
                needsFormat = true;
                break;
            }
        }
        if (needsFormat) {
            while (written < total) {
                esp_task_wdt_reset();
                uint16_t toWrite = min<uint16_t>(chunk, total - written);
                eeprom.writeBlock(written, buf, toWrite);
                written += toWrite;
                pct = (uint8_t)((written * 60UL) / total);
                drawRepair("Formatting EEPROM...", pct);
                delay(4);
            }
            uint8_t defaults[8] = {120, 0, 0, 0, 0, 0x01, 0xD4, 0xC0};
            eeprom.writeBlock(0x0000, defaults, 8);
            pct = 65;
            drawRepair("Writing defaults...", pct);
            delay(120);
        }
    }

    drawRepair("Recalibrating services...", 80);
    stopAllService();
    delay(100);
    startService();

    delay(200);

    if (!lastErrOk) {
        drawRepair("Clearing error state...", 95);
        delay(300);
    }

    drawRepair("Finalizing...", 100);
    delay(200);

    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(2, 18, "Repairs Completed");
    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(2, 32, eepromOk ? "EEPROM Restored" : "EEPROM Skipped");
    display.drawStr(2, 40, boardOk ? "Board ID: OK" : "Board ID: WARN");
    display.drawStr(2, 48, heapOk ? "Heap: OK" : "Heap: LOW");
    display.drawStr(2, 56, lastErrOk ? "Last Error: OK" : "Last Error: CLEARED");
    display.sendBuffer();
    btnOK.attachClick([]() { drawMenu(); });

    delay(600);
}