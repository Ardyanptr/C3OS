#include "BLEScanner.h"

#include <NimBLEDevice.h>

static bool _isExit_BLEScanner = false;

static void handleExitBLEScanner() {
    _isExit_BLEScanner = true;
}

void runBLEScanner() {
    _isExit_BLEScanner = false;
    btnOK.attachClick(handleExitBLEScanner);

    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

    if (!NimBLEDevice::isInitialized()) {
        NimBLEDevice::init("C3OS-Scanner");
        NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    }

    NimBLEScan* pNimBLEScan = NimBLEDevice::getScan();
    pNimBLEScan->setActiveScan(true);
    pNimBLEScan->setInterval(100);
    pNimBLEScan->setWindow(60);
    pNimBLEScan->setMaxResults(0);

    while (!_isExit_BLEScanner) {
        appHeartBeat();

        btnUp.tick();
        btnDown.tick();
        btnOK.tick();
        btnAction.tick();

        display.clearBuffer();
        display.setFont(u8g2_font_5x7_tf);
        display.drawStr(0, 7, "NimBLE Scanner - Scan...");
        display.drawHLine(0, 9, 128);
        display.sendBuffer();

        pNimBLEScan->start(1, false);
        NimBLEScanResults results = pNimBLEScan->getResults();
        int count = results.getCount();

        display.clearBuffer();
        display.drawHLine(0, 9, 128);
        display.setFont(u8g2_font_5x7_tf);
        display.setCursor(0, 7);
        display.print("FOUND: ");
        display.print(count);
        display.print(" HP:");
        display.print(ESP.getFreeHeap() / 1024);
        display.print("K");

        for (int i = 0; i < count && i < 4; i++) {
            const NimBLEAdvertisedDevice* device = results.getDevice(i);

            int rssi = device->getRSSI();
            String name = device->getName().c_str();
            String addr = device->getAddress().toString().c_str();

            if (name == "") name = "<Hidden>";

            int y = 19 + (i * 11);
            display.setFont(u8g2_font_4x6_tf);
            display.setCursor(0, y);
            display.print(name.substring(0, 16));

            int bar = map(rssi, -100, -30, 0, 25);
            display.drawFrame(100, y - 4, 27, 5);
            display.drawBox(101, y - 3, constrain(bar, 0, 25), 3);

            display.setCursor(5, y + 6);
            display.print(addr);
            display.print(" [");
            display.print(rssi);
            display.print("]");
        }

        display.setFont(u8g2_font_4x6_tf);
        display.drawStr(0, 64, "Press OK to exit");
        display.sendBuffer();

        pNimBLEScan->clearResults();
        delay(100);
        yield();
    }

    pNimBLEScan->stop();
    btnOK.attachClick(nullptr);
}