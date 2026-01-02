#pragma once
#include <Arduino.h>

#include "include/all_.h"

extern bool isOnErrorFlag;

static inline String crashID() {
    unsigned long t = millis();
    size_t heap = ESP.getFreeHeap();
    int h3 = heap % 1000;
    uint8_t code = (uint8_t)esp_reset_reason();

    return "0x" + String(code, HEX) + String(h3, HEX) + String(t, HEX);
}

static inline String createDump(String pCap, String mill, String cap, String pca) {
    String dump = pCap + "\n" + mill + "\n" + cap + "\n" + pca + "\n";
    return dump;
}

static inline void showCrashInfo() {
    size_t currentHeap = ESP.getFreeHeap();
    uint8_t errorCode = (uint8_t)esp_reset_reason();

    String pCap = "PCAP: <" + crashID() + ">";
    String mill = "MILL: <0x" + String(millis()) + ">";
    String cap = "CAP  : <0x" + String(currentHeap % 1000) + ">";
    String pca = "PC-A: <0x" + String(errorCode) + ">";

    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    display.drawXBM(0, 0, 11, 11, image_Untitled_1_bits);

    display.setFont(u8g2_font_profont11_tr);
    display.drawStr(13, 10, "C3OS BLSOD");

    display.setFont(u8g2_font_4x6_tr);

    char reset_buffer[64];
    snprintf(reset_buffer, sizeof(reset_buffer), "Reason: %d", esp_reset_reason());

    char heap_buffer[32];
    snprintf(heap_buffer, sizeof(heap_buffer), "Current Heap: %d", currentHeap % 1000);

    display.drawStr(1, 17, reset_buffer);
    display.drawStr(1, 23, heap_buffer);
    display.sendBuffer();

    display.drawStr(1, 63, pCap.c_str());
    display.drawStr(1, 57, mill.c_str());
    display.drawStr(1, 51, cap.c_str());
    display.drawStr(1, 45, pca.c_str());

    display.sendBuffer();
    btnOK.attachClick([]() { isOnErrorFlag = false; delay(50); C3OSRecovery(); });

    while (isOnErrorFlag) {
        btnOK.tick();

        delay(2);
        yield();
    }
}