#include "Settings.h"

#include <WiFi.h>

Settings* Settings::instance = nullptr;

Settings::Settings() {
    instance = this;
}

// ISR/Callback Gateways
uint8_t calculateChecksum(const Settings::Data& d) {
    const uint8_t* ptr = (const uint8_t*)&d;
    uint8_t crc = 0;
    // Hitung semua kecuali byte terakhir (checksum itu sendiri)
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
        Wire.begin(8, 9);
        Wire.setClock(100000);

        if (myMem.begin(0x50)) {
            load();
            apply();
            initialized = true;
            Serial.println("EEPROM Initialized for the first time.");
        } else {
            Serial.println("EEPROM Hardware Not Found!");
        }
    }

    // Bagian bawah ini tetap dijalankan setiap kali menu dibuka
    flagUp = flagDown = flagOK = flagExit = false;
    cursor = 0;
    editing = false;

    btnUp.attachClick(onUp);
    btnDown.attachClick(onDown);
    btnOK.attachClick(onOK);
    btnOK.attachLongPressStart(onExit);
}

void Settings::load() {
    Data temp = {0};
    bool success = false;

    // Coba baca maksimal 3 kali jika gagal
    for (int i = 0; i < 3; i++) {
        myMem.get(0, temp);

        bool sigValid = (temp.signature == 0x53455431);
        bool crcValid = (temp.checksum == calculateChecksum(temp));

        if (sigValid && crcValid) {
            data = temp;
            if (data.oledContrast < 20) data.oledContrast = 20;
            Serial.println("EEPROM Load Success!");
            success = true;
            break;
        }
        delay(10);  // Kasih jeda sedikit antar percobaan
    }

    if (!success) {
        Serial.println("EEPROM Load Failed after 3 retries! Using defaults in RAM.");
        // Isi RAM saja, JANGAN panggil save() di sini!
        data.signature = 0x53455431;
        data.bluetooth = false;
        data.wifi = true;
        data.wifiPower = 80;
        data.oledContrast = 150;
        data.sleepTimeout = 60000;
        // save(); <-- HAPUS BARIS INI, jangan overwrite data asli di chip jika cuma gagal baca
    }
}

void Settings::save() {
    data.signature = 0x53455431;
    data.checksum = calculateChecksum(data);

    myMem.put(0, data);
    Serial.println("Data saved with chksum");
}

void Settings::loadSettings() {
    load();
    apply();
}

void Settings::apply() {
    display.setContrast(data.oledContrast);

    // FIX ERROR MAX: Pakai <uint32_t> agar tipe data sinkron
    // Memastikan sleepTimeout minimal 10 detik (10000ms)
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

    // Animasi tetap pakai float
    float targetY = cursor * itemHeight;
    animCursorY += (targetY - animCursorY) * 0.3f;

    // Box seleksi mengikuti animasi
    display.setDrawColor(1);
    display.drawRBox(0, (int)animCursorY + 1, 128, 12, 2);

    const char* labels[] = {"Bluetooth", "WiFi Status", "TX Power", "Brightness", "Sleep"};

    for (uint8_t i = 0; i < 5; i++) {
        // PENTING: Gunakan 'cursor' asli untuk warna teks, bukan hasil animasi
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

        // Tambah visual bracket [] kalau lagi editing agar jelas
        if (editing && i == cursor) {
            char editBuf[25];
            snprintf(editBuf, sizeof(editBuf), "[%s]", buf);
            display.drawStr(123 - display.getStrWidth(editBuf), 11 + (i * itemHeight), editBuf);
        } else {
            display.drawStr(123 - display.getStrWidth(buf), 11 + (i * itemHeight), buf);
        }
    }

    display.setDrawColor(1);
    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(0, 63, editing ? "EDITING MODE - PRESS OK TO SAVE" : "HOLD OK TO EXIT | PRESS TO EDIT");
    display.sendBuffer();
}

void Settings::handleInput() {
    if (flagUp) {
        flagUp = false;
        if (!editing) {
            cursor = (cursor == 0) ? 4 : cursor - 1;
        } else {
            if (cursor == 2) data.wifiPower = (data.wifiPower >= 100) ? 100 : data.wifiPower + 10;
            if (cursor == 3) {
                data.oledContrast = (data.oledContrast >= 255) ? 255 : data.oledContrast + 15;
                display.setContrast(data.oledContrast);
            }
            if (cursor == 4) data.sleepTimeout += 10000;
        }
    }

    if (flagDown) {
        flagDown = false;
        if (!editing) {
            cursor = (cursor == 4) ? 0 : cursor + 1;
        } else {
            if (cursor == 2) data.wifiPower = (data.wifiPower <= 10) ? 0 : data.wifiPower - 10;
            if (cursor == 3) {
                data.oledContrast = (data.oledContrast <= 20) ? 5 : data.oledContrast - 15;
                display.setContrast(data.oledContrast);
            }
            if (cursor == 4) {
                // Gunakan casting manual yang lebih bersih
                uint32_t current = data.sleepTimeout;
                data.sleepTimeout = (current <= 10000) ? 10000 : current - 10000;
            }
        }
    }

    if (flagOK) {
        flagOK = false;
        if (cursor < 2) {  // Toggle Bluetooth/Wifi langsung
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
        isRunning = false;  // Memutus loop while di run()
    }
}

void Settings::run() {
    isRunning = true;
    while (isRunning) {
        appHeartBeat();

        // Tick button agar tetap responsif di dalam loop
        btnUp.tick();
        btnDown.tick();
        btnOK.tick();

        handleInput();
        draw();

        delay(10);  // Sekitar 100 FPS
    }

    drawMenu();
}

Settings::Data& Settings::get() { return data; }