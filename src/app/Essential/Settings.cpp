#include "Settings.h"

#include <WiFi.h>

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
        data.aodPin = 4;
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
}

void Settings::draw() {
    display.clearBuffer();
    float targetY = cursor * itemHeight;
    animCursorY += (targetY - animCursorY) * 0.3f;

    display.setDrawColor(1);
    display.drawRBox(0, (int)animCursorY + 1, 128, 12, 2);

    const char* labels[] = {"Bluetooth", "WiFi Status", "TX Power", "Brightness", "Sleep", "AOD Wake"};

    for (uint8_t i = 0; i < 6; i++) {
        display.setDrawColor(i == cursor ? 0 : 1);
        display.setFont(u8g2_font_6x10_tf);
        display.drawStr(5, 11 + (i * itemHeight), labels[i]);

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

        if (editing && i == cursor) {
            char editBuf[25];
            snprintf(editBuf, sizeof(editBuf), "[%s]", buf);
            display.drawStr(123 - display.getStrWidth(editBuf), 11 + (i * itemHeight), editBuf);
        } else {
            display.drawStr(123 - display.getStrWidth(buf), 11 + (i * itemHeight), buf);
        }
    }

    display.sendBuffer();
}

void Settings::handleInput() {
    if (flagUp) {
        flagUp = false;
        if (!editing) {
            cursor = (cursor == 0) ? 5 : cursor - 1;
        } else {
            if (cursor == 2) data.wifiPower = (data.wifiPower >= 100) ? 100 : data.wifiPower + 10;
            if (cursor == 3) {
                data.oledContrast = (data.oledContrast >= 255) ? 255 : data.oledContrast + 15;
                display.setContrast(data.oledContrast);
            }
            if (cursor == 4) data.sleepTimeout += 10000;
            if (cursor == 5) data.aodPin = (data.aodPin >= 4) ? 0 : data.aodPin + 1;
        }
    }

    if (flagDown) {
        flagDown = false;
        if (!editing) {
            cursor = (cursor == 5) ? 0 : cursor + 1;
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

            if (cursor == 5) data.aodPin = (data.aodPin <= 0) ? 4 : data.aodPin - 1;
        }
    }

    if (flagOK) {
        flagOK = false;
        if (cursor < 2) {
            if (cursor == 0) data.bluetooth = !data.bluetooth;
            if (cursor == 1) data.wifi = !data.wifi;
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