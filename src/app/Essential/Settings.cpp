#include "Settings.h"

#include <WiFi.h>

#include "system16/esp826.h"

Settings* Settings::instance = nullptr;

Settings::Settings() {
    instance = this;
}

uint8_t calculateChecksum(const Settings::Data& d) {
    const uint8_t* ptr = (const uint8_t*)&d;
    uint8_t crc = 0;
    for (size_t i = 0; i < sizeof(Settings::Data) - 1; i++) {
        crc ^= ptr[i];
    }
    return crc;
}

void Settings::onUp() { instance->flagUp = true; }
void Settings::onDown() { instance->flagDown = true; }
void Settings::onOK() { instance->flagOK = true; }
void Settings::onExit() { instance->flagExit = true; }

void Settings::begin() {
    static bool initialized = false;

    if (!initialized) {
        if (LittleFS.begin(true)) {
            load();
            apply();
            initialized = true;
        }
    }

    flagUp = flagDown = flagOK = flagExit = false;
    cursor = 0;
    editing = false;

    btnUp.attachClick(onUp);
    btnDown.attachClick(onDown);
    btnOK.attachClick(onOK);
    btnOK.attachLongPressStart(onExit);
}

void Settings::load() {
    bool success = false;
    if (LittleFS.exists(SETTINGS_PATH)) {
        File file = LittleFS.open(SETTINGS_PATH, "r");
        if (file) {
            if (file.read((uint8_t*)&data, sizeof(Data)) == sizeof(Data)) {
                bool sigValid = (data.signature == 0x53455431);
                bool crcValid = (data.checksum == calculateChecksum(data));
                if (sigValid && crcValid) {
                    success = true;
                    Serial.println(data.aodPin);
                }
            }
            file.close();
        }
    }

    if (!success) {
        data.signature = 0x53455431;
        data.bluetooth = false;
        data.wifi = true;
        data.wifiPower = 80;
        data.oledContrast = 150;
        data.sleepTimeout = 60000;
        data.aodPin = 3;
        data.memFusion = 0;
        data.cpuFrequency = 160;
        data.gameMode = false;
        data.fastboot = false;
        data.checksum = calculateChecksum(data);
    }
}

void Settings::save() {
    data.signature = 0x53455431;
    data.checksum = calculateChecksum(data);

    File file = LittleFS.open(SETTINGS_PATH, "w");
    if (file) {
        file.write((const uint8_t*)&data, sizeof(Data));
        file.close();
    }
}

void Settings::loadSettings() {
    load();
    apply();
}

void Settings::apply() {
    display.setContrast(data.oledContrast);

    uint32_t safeTimeout = max<uint32_t>(10000u, data.sleepTimeout);

    if (data.wifi) {
        WiFi.mode(WIFI_STA);
        WiFi.setTxPower((wifi_power_t)(data.wifiPower / 5));
    } else {
        WiFi.mode(WIFI_OFF);
    }

    if (data.memFusion == 0) {
        Serial1.println("avr32:fusion-off");
    } else if (data.memFusion == 1) {
        Serial1.println("avr32:fusion-on:32");
    } else if (data.memFusion == 2) {
        Serial1.println("avr32:fusion-on:64");
    }

    setCpuFrequencyMhz(data.cpuFrequency);
}

void Settings::draw() {
    display.clearBuffer();

    float targetCursorY = cursor * itemHeight;
    animCursorY += (targetCursorY - animCursorY) * 0.3f;

    static float scrollY = 0;
    float targetScrollY = 0;
    if (cursor > 3) {
        targetScrollY = (cursor - 3) * itemHeight;
    }
    scrollY += (targetScrollY - scrollY) * 0.2f;

    const char* labels[] = {"Bluetooth", "WiFi Status", "TX Power", "Brightness", "Sleep", "AOD Wake", "Mem Fusion", "CPU Freq", "Game Mode", "Fastboot"};
    uint8_t totalItems = sizeof(labels) / sizeof(labels[0]);

    display.setDrawColor(1);
    display.drawRBox(0, (int)(animCursorY - scrollY) + 1, 128, 12, 2);

    for (uint8_t i = 0; i < totalItems; i++) {
        int yPos = 11 + (i * itemHeight) - (int)scrollY;

        if (yPos < -10 || yPos > 74) continue;

        display.setDrawColor(i == cursor ? 0 : 1);
        display.setFont(u8g2_font_6x10_tf);
        display.drawStr(5, yPos, labels[i]);

        char buf[20];
        if (i == 0)
            strcpy(buf, data.bluetooth ? "ON" : "OFF");
        else if (i == 1)
            strcpy(buf, data.wifi ? "ON" : "OFF");
        else if (i == 2)
            snprintf(buf, sizeof(buf), "%d%%", data.wifiPower);
        else if (i == 3)
            snprintf(buf, sizeof(buf), "%d", data.oledContrast);
        else if (i == 4)
            snprintf(buf, sizeof(buf), "%lus", (unsigned long)(data.sleepTimeout / 1000));
        else if (i == 5)
            snprintf(buf, sizeof(buf), "GPIO %d", data.aodPin);
        else if (i == 6) {
            if (data.memFusion == 0)
                strcpy(buf, "OFF");
            else if (data.memFusion == 1)
                strcpy(buf, "32KB");
            else if (data.memFusion == 2)
                strcpy(buf, "64KB");
        } else if (i == 7) {
            snprintf(buf, sizeof(buf), "%dMHz", data.cpuFrequency);
        } else if (i == 8) {
            strcpy(buf, data.gameMode ? "ON" : "OFF");
        } else if (i == 9) {
            strcpy(buf, data.fastboot ? "ON" : "OFF");
        }

        if (editing && i == cursor) {
            char editBuf[30];
            snprintf(editBuf, sizeof(editBuf), "[%s]", buf);
            display.drawStr(123 - display.getStrWidth(editBuf), yPos, editBuf);
        } else {
            display.drawStr(123 - display.getStrWidth(buf), yPos, buf);
        }
    }
    display.sendBuffer();
}

void Settings::handleInput() {
    if (flagUp) {
        flagUp = false;
        if (!editing) {
            cursor = (cursor == 0) ? 9 : cursor - 1;
        } else {
            if (cursor == 2) data.wifiPower = (data.wifiPower >= 100) ? 100 : data.wifiPower + 10;
            if (cursor == 3) {
                data.oledContrast = (data.oledContrast >= 255) ? 255 : data.oledContrast + 15;
                display.setContrast(data.oledContrast);
            }
            if (cursor == 4) data.sleepTimeout += 10000;
            if (cursor == 5) data.aodPin = (data.aodPin >= 3) ? 0 : data.aodPin + 1;
            if (cursor == 6) data.memFusion = (data.memFusion >= 2) ? 0 : data.memFusion + 1;
            if (cursor == 7) data.cpuFrequency = (data.cpuFrequency >= 160) ? 40 : data.cpuFrequency + 40;
        }
    }

    if (flagDown) {
        flagDown = false;
        if (!editing) {
            cursor = (cursor == 9) ? 0 : cursor + 1;
        } else {
            if (cursor == 2) data.wifiPower = (data.wifiPower <= 10) ? 0 : data.wifiPower - 10;
            if (cursor == 3) {
                data.oledContrast = (data.oledContrast <= 20) ? 20 : data.oledContrast - 15;
                display.setContrast(data.oledContrast);
            }
            if (cursor == 4) {
                uint32_t current = data.sleepTimeout;
                data.sleepTimeout = (current <= 10000) ? 10000 : current - 10000;
            }

            if (cursor == 5) data.aodPin = (data.aodPin <= 0) ? 3 : data.aodPin - 1;
            if (cursor == 6) data.memFusion = (data.memFusion == 0) ? 2 : data.memFusion - 1;
            if (cursor == 7) data.cpuFrequency = (data.cpuFrequency <= 40) ? 160 : data.cpuFrequency - 40;
        }
    }

    if (flagOK) {
        flagOK = false;
        if (cursor == 0 || cursor == 1 || cursor == 8 || cursor == 9) {
            if (cursor == 0) data.bluetooth = !data.bluetooth;
            if (cursor == 1) data.wifi = !data.wifi;
            if (cursor == 8) data.gameMode = !data.gameMode;
            if (cursor == 9) data.fastboot = !data.fastboot;

            save();
            apply();
        } else {
            editing = !editing;
            if (!editing) {
                save();
                apply();
            }
        }
    }

    if (flagExit) {
        flagExit = false;
        isRunning = false;
    }
}

void Settings::run() {
    isRunning = true;
    while (isRunning) {
        appHeartBeat();
        btnUp.tick();
        btnDown.tick();
        btnOK.tick();
        handleInput();
        draw();
        delay(10);
    }
    drawMenu();
}

Settings::Data& Settings::get() { return data; }