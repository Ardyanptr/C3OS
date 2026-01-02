#pragma once
#include <Arduino.h>

#include "config/var_declare.h"

inline bool eepWriteTest() {
    Wire.beginTransmission(0x50);
    Wire.write(0x00);
    Wire.write(0x10);
    Wire.write(0xAB);
    return Wire.endTransmission() == 0;
}

inline bool dumpToEEPROM(AT24C256& e, uint16_t addr, const String& dump) {
    uint16_t len = dump.length();
    uint8_t buf[len];

    dump.getBytes(buf, len + 1);
    return e.writeBlock(addr, buf, len);
}

inline void C3OSRecovery() {
    size_t currentHeap = ESP.getFreeHeap();
    uint8_t errorCode = (uint8_t)esp_reset_reason();

    String pCap = "PCAP: <" + crashID() + ">";
    String mill = "MILL: <0x" + String(millis()) + ">";
    String cap = "CAP  : <0x" + String(currentHeap % 1000) + ">";
    String pca = "PC-A: <0x" + String(errorCode) + ">";

    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);
    display.sendBuffer();

    display.drawXBM(1, 1, 9, 8, image_Alert_bits);

    display.setFont(u8g2_font_5x8_tr);
    display.drawStr(13, 9, "C3OS Recovery");
    display.sendBuffer();

    display.drawStr(1, 21, "Checking EEPROM...");
    display.sendBuffer();

    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(0x50);
        if (Wire.endTransmission() == 0) {
            eep.begin();
            if (eepWriteTest) {
                display.drawStr(115, 21, "OK");
                display.sendBuffer();
            }
        }
    }

    display.drawStr(1, 29, "Dumping...");
    display.sendBuffer();
    String dump = createDump(pCap, mill, cap, pca);
    if (dumpToEEPROM(eep, 0, dump)) {
        display.drawStr(115, 29, "OK");
        display.sendBuffer();
    }

    display.drawStr(1, 37, "Checking Device...");
    display.sendBuffer();

    const char* chipModel = ESP.getChipModel();
    uint8_t chipRev = ESP.getChipRevision();

    String chipRevision = String(chipRev);
    String chipModels = String(chipModel);

    if (chipRevision == String(BOARD_REV) || chipModels == String(BOARD_ATTACHED)) {
        display.drawStr(115, 37, "OK");
        display.sendBuffer();
    }

    display.drawStr(1, 45, "Reloading component");
    display.sendBuffer();
    stopAllService();
    delay(500);
    startService();
    display.drawStr(115, 45, "OK");
    display.sendBuffer();

    display.drawStr(1, 63, "Reboot?");
    display.drawStr(105, 63, "WAIT");
    display.sendBuffer();

    btnOK.attachClick([]() { ESP.restart(); });
    while (true) {
        btnOK.tick();

        delay(2);
        yield();
    }

    display.sendBuffer();
}