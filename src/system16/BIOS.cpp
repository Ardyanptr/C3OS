#include <Arduino.h>
#include "BIOS.h"

#include <LittleFS.h>
#include <system16/boot/boot.h>

bool upFlag_BIOS = false;
bool downFlag_BIOS = false;
bool okFlag_BIOS = false;
bool actionFlag_BIOS = false;
bool okLong_BIOS = false;

void drawProgressRing(int x, int y, float progress, float radius) {
    int steps = (int)(progress * 24);
    for (int i = 0; i < steps; i++) {
        float a = (i / 24.0) * 6.28 - 1.57;
        int px = x + cos(a) * radius;
        int py = y + sin(a) * radius;
        display.drawPixel(px, py);
    }
}

void runBIOS_SAFE_MODE() {
    for (float t = 0; t <= 1; t += 0.05) {
        display.clearBuffer();
        float y = lerp(64, 28, t);
        display.setFont(u8g2_font_6x10_tf);
        display.drawStr(25, y, "SAFE MODE");
        display.drawFrame(10, y - 12, 108, 24);
        display.sendBuffer();
        delay(16);
    }

    delay(500);
    boot_mode = BOOT_SAFE;
    esp_sleep_enable_timer_wakeup(1ULL * 1000000);
    esp_deep_sleep_start();
}

void bios_i2c_scan() {
    display.clearBuffer();
    display.setFont(u8g2_font_5x8_tf);
    display.drawStr(35, 10, "I2C SCAN");
    display.sendBuffer();

    Wire.begin();
    int found = 0;
    float scanProgress = 0;

    for (uint8_t addr = 1; addr < 127; addr++) {
        esp_task_wdt_reset();
        scanProgress = addr / 127.0;

        display.clearBuffer();
        display.drawStr(35, 10, "I2C SCAN");
        drawProgressRing(64, 35, scanProgress, 15);

        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            display.setCursor(10, 55 + found * 8);
            display.print("0x");
            if (addr < 16) display.print("0");
            display.print(addr, HEX);
            found++;
            if (found > 2) break;
        }

        display.sendBuffer();
        delay(10);
    }

    if (found == 0) {
        display.setCursor(30, 50);
        display.print("No devices");
        display.sendBuffer();
    }

    delay(2000);
}

void bios_eeprom_test() {
    AT24C256 eeprom(0x50);
    eeprom.begin();

    display.clearBuffer();
    display.drawStr(25, 10, "EEPROM TEST");
    display.sendBuffer();

    uint8_t testData[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    eeprom.writeBlock(0x0000, testData, 4);

    float p = 0;
    while (p < 1) {
        esp_task_wdt_reset();
        p += 0.05;
        display.clearBuffer();
        display.drawStr(25, 10, "EEPROM TEST");
        display.drawStr(20, 30, "Writing...");
        drawProgressRing(64, 40, p, 12);
        display.sendBuffer();
        delay(50);
    }

    delay(100);

    uint8_t readData[4];
    eeprom.readBlock(0x0000, readData, 4);

    bool pass = (memcmp(testData, readData, 4) == 0);

    display.clearBuffer();
    display.drawStr(25, 10, "EEPROM TEST");
    display.setCursor(35, 35);
    display.print(pass ? "PASS" : "FAIL");
    display.drawFrame(30, 25, 68, 20);
    display.sendBuffer();
    delay(1500);
}

void bios_benchmark() {
    display.clearBuffer();
    display.drawStr(30, 10, "BENCHMARK");
    display.sendBuffer();

    unsigned long start = micros();

    volatile uint32_t x = 0;
    for (uint32_t i = 0; i < 100000; i++) x += i * 31;

    uint8_t* buf1 = (uint8_t*)malloc(1024);
    uint8_t* buf2 = (uint8_t*)malloc(1024);
    for (int i = 0; i < 1000; i++) memcpy(buf2, buf1, 1024);
    free(buf1);
    free(buf2);

    unsigned long elapsed = micros() - start;
    float score = 1000000.0 / elapsed;

    display.clearBuffer();
    display.drawStr(30, 10, "BENCHMARK");
    display.setCursor(25, 30);
    display.print((int)(score * 100));
    display.print(" pts");
    display.setCursor(20, 45);
    display.print(elapsed / 1000);
    display.print(" ms");
    display.sendBuffer();
    delay(2000);
}

void bios_fs_check() {
    display.clearBuffer();
    display.drawStr(30, 10, "FS CHECK");
    display.sendBuffer();

    if (!LittleFS.begin(true)) {
        display.setCursor(25, 40);
        display.print("MOUNT FAIL");
        display.sendBuffer();
        delay(1500);
        return;
    }

    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();

    int fileCount = 0;
    File root = LittleFS.open("/");
    if (root) {
        File file = root.openNextFile();
        while (file) {
            fileCount++;
            file = root.openNextFile();
        }
        root.close();
    }

    display.clearBuffer();
    display.drawStr(30, 10, "FS CHECK");
    display.setCursor(10, 28);
    display.print("Total: ");
    display.print(totalBytes / 1024);
    display.print("KB");
    display.setCursor(10, 40);
    display.print("Used: ");
    display.print(usedBytes / 1024);
    display.print("KB");
    display.setCursor(10, 52);
    display.print("Files: ");
    display.print(fileCount);
    display.sendBuffer();
    delay(2500);
}

void bios_kill_switch() {
    display.clearBuffer();
    display.drawStr(25, 10, "FACTORY RESET");
    display.setCursor(15, 30);
    display.print("Hold OK 3s");
    display.drawStr(10, 50, "Action: Cancel");
    display.sendBuffer();

    unsigned long holdStart = millis();
    bool confirmed = false;

    while (millis() - holdStart < 4000) {
        esp_task_wdt_reset();
        if (digitalRead(BUTTON_OK) == LOW) {
            if (millis() - holdStart > 3000) {
                confirmed = true;
                break;
            }

            int w = map(millis() - holdStart, 0, 3000, 0, 100);
            display.drawBox(14, 38, w, 6);
            display.sendBuffer();
        } else {
            holdStart = millis();
        }
        delay(50);
    }

    if (!confirmed) return;

    for (float t = 0; t <= 1; t += 0.1) {
        display.clearBuffer();
        drawProgressRing(64, 40, t, 15);
        display.sendBuffer();
        delay(100);
    }

    LittleFS.format();
    EEPROM.begin(512);
    for (int i = 0; i < 512; i++) EEPROM.write(i, 0);
    EEPROM.commit();

    display.clearBuffer();
    display.drawStr(35, 35, "DONE");
    display.sendBuffer();
    delay(1000);
}

void runBIOS() {
    AT24C256 eeprom(0x50);
    eeprom.begin();

    float corePulse = 0;
    for (int frame = 0; frame < 60; frame++) {
        display.clearBuffer();

        corePulse = (sin(frame * 0.2) + 1) / 2;
        int r = 3 + (int)(corePulse * 5);
        for (int y = -r; y <= r; y++) {
            for (int x = -r; x <= r; x++) {
                if (x * x + y * y <= r * r) display.drawPixel(64 + x, 32 + y);
            }
        }

        float orbit = frame * 0.15;
        int ox = 64 + cos(orbit) * 20;
        int oy = 32 + sin(orbit) * 12;
        display.drawDisc(ox, oy, 1);

        if (frame > 30) {
            display.setFont(u8g2_font_4x6_tr);
            display.drawStr(40, 55, "DIAGNOSTICS");
        }

        display.sendBuffer();
        delay(16);
    }

    const char* items[] = {
        "EXIT",
        "REBOOT",
        "SAFE MODE",
        "FACTORY RESET",
        "I2C SCAN",
        "EEPROM TEST",
        "FILESYSTEM",
        "BENCHMARK",
        "BRIGHTNESS",
        "SLEEP TIMER"};

    const uint8_t itemCount = sizeof(items) / sizeof(items[0]);
    int sel = 0;
    float animSel = 0;
    float hexPulse = 0;

    int brightness = 150;
    int sleepMin = 1;

    btnUp.attachClick([]() { upFlag_BIOS = true; });
    btnDown.attachClick([]() { downFlag_BIOS = true; });
    btnOK.attachClick([]() { okFlag_BIOS = true; });
    btnAction.attachClick([]() { actionFlag_BIOS = true; });
    btnOK.attachLongPressStart([]() { okLong_BIOS = true; });

    bool exitBIOS = false;
    unsigned long lastActivity = millis();

    while (!exitBIOS) {
        esp_task_wdt_reset();
        if (upFlag_BIOS) {
            sel = (sel - 1 + itemCount) % itemCount;
            upFlag_BIOS = false;
            lastActivity = millis();
        }
        if (downFlag_BIOS) {
            sel = (sel + 1) % itemCount;
            downFlag_BIOS = false;
            lastActivity = millis();
        }
        if (actionFlag_BIOS) {
            exitBIOS = true;
            break;
        }

        if (okFlag_BIOS) {
            okFlag_BIOS = false;
            lastActivity = millis();

            switch (sel) {
                case 0:
                    exitBIOS = true;
                    break;
                case 1:
                    display.clearBuffer();
                    display.setFont(u8g2_font_6x10_tf);
                    display.drawStr(35, 35, "REBOOT");
                    display.sendBuffer();
                    delay(500);
                    esp_restart();
                    break;
                case 2:
                    runBIOS_SAFE_MODE();
                    break;
                case 3:
                    bios_kill_switch();
                    break;
                case 4:
                    bios_i2c_scan();
                    break;
                case 5:
                    bios_eeprom_test();
                    break;
                case 6:
                    bios_fs_check();
                    break;
                case 7:
                    bios_benchmark();
                    break;
                case 8:

                    brightness = (brightness >= 250) ? 50 : brightness + 50;
                    display.setContrast(brightness);
                    break;
                case 9:
                    sleepMin = (sleepMin >= 10) ? 1 : sleepMin + 1;
                    SLEEP_TIMEOUT = sleepMin * 60000;
                    break;
            }
        }

        if (okLong_BIOS) {
            exitBIOS = true;
            okLong_BIOS = false;
        }

        animSel += (sel - animSel) * 0.2;
        hexPulse = (sin(millis() / 300.0) + 1) / 2;

        display.clearBuffer();

        display.drawBox(0, 0, 128, 11);
        display.setDrawColor(0);
        display.setFont(u8g2_font_5x8_tf);
        display.drawStr(2, 8, "C3 SYSTEM RECOVERY");
        display.setDrawColor(1);

        const int visibleItems = 4;
        int offset = (int)animSel - 1;
        offset = constrain(offset, 0, itemCount - visibleItems);

        for (int i = 0; i < visibleItems; i++) {
            int idx = offset + i;
            if (idx >= itemCount) break;

            float yBase = 18 + i * 12;
            float y = yBase + (animSel - idx) * 2;

            bool selected = (idx == sel);

            if (selected) {
                display.drawBox(0, y - 1, 100, 11);
                display.setDrawColor(0);
                display.setFont(u8g2_font_6x10_tf);
                display.drawStr(6, y + 7, items[idx]);
                display.setDrawColor(1);

                display.drawDisc(108, y + 4, 2);
            } else {
                display.setFont(u8g2_font_5x8_tf);
                display.drawStr(4, y + 6, items[idx]);
            }
        }

        display.drawVLine(100, 14, 50);

        display.setFont(u8g2_font_4x6_tr);
        if (sel == 8) {
            display.drawStr(104, 22, "LVL");
            char buf[4];
            snprintf(buf, sizeof(buf), "%d", brightness / 25);
            display.setFont(u8g2_font_6x10_tf);
            display.drawStr(104, 36, buf);
            display.drawFrame(102, 42, 24, 4);
            display.drawBox(104, 44, brightness / 11, 1);
        } else if (sel == 9) {
            display.drawStr(104, 22, "MIN");
            char buf[4];
            snprintf(buf, sizeof(buf), "%d", sleepMin);
            display.setFont(u8g2_font_6x10_tf);
            display.drawStr(104, 36, buf);
        } else if (sel == 4) {
            display.drawStr(102, 25, "SCAN");
            display.drawStr(102, 40, "BUS");
        } else {
            display.drawStr(102, 22, "V4.2");
            display.setCursor(102, 35);
            display.print(millis() / 1000);
            display.print("s");
        }

        if (itemCount > visibleItems) {
            int barH = 40 * visibleItems / itemCount;
            int barY = 14 + (offset * (50 - barH) / (itemCount - visibleItems));
            display.drawFrame(124, 14, 3, 50);
            display.drawBox(125, barY, 1, barH);
        }

        display.sendBuffer();

        btnUp.tick();
        btnDown.tick();
        btnOK.tick();
        btnAction.tick();

        delay(16);
    }

    btnUp.attachClick(nullptr);
    btnDown.attachClick(nullptr);
    btnOK.attachClick(nullptr);
    btnAction.attachClick(nullptr);

    for (float t = 0; t <= 1; t += 0.1) {
        display.clearBuffer();
        float alpha = 1 - t;
        int r = (int)(alpha * 40);
        display.drawCircle(64, 32, r);
        display.sendBuffer();
        delay(30);
    }

    boot();
}