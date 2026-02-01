#include "WiFiConnect.h"

#include <Arduino.h>
#include <WiFiManager.h>

// UI Constants - Diselaraskan dengan Menu Utama
const int WC_ITEM_HEIGHT = 16;
const int WC_TEXT_OFFSET = 13;
const int WC_START_Y = 18;

// Menu State
#define MAX_SAVED 2

int wifiMenuIndex = 0;

String savedSSIDs[MAX_SAVED] = {"_ARDASYAH", "ardyan"};
String savedPass[MAX_SAVED] = {"Satunusasatubangsa", "ardyann"};

String ssid, pass;

bool isWiFiConnectRunning = true;

// --- UTILITY UI ---

void showStatus(String title, String msg, bool isError = false) {
    display.clearBuffer();
    display.setFont(u8g2_font_7x14_tr);
    display.setDrawColor(1);
    display.drawStr(15, 28, title.c_str());

    display.setFont(u8g2_font_5x7_tr);
    display.drawStr(15, 42, msg.c_str());

    if (isError) display.drawFrame(0, 0, 128, 64);  // Frame tambahan kalau error
    display.sendBuffer();
}

void showConnectingUI(String label) {
    static int dotCount = 0;
    dotCount = (dotCount + 1) % 4;
    String dots = label;
    for (int i = 0; i < dotCount; i++) dots += ".";

    display.clearBuffer();
    display.setFont(u8g2_font_7x14_tr);
    display.setDrawColor(1);
    display.drawStr(20, 36, dots.c_str());
    display.sendBuffer();
}

// --- LOGIC FUNCTIONS ---

void ConnectToWiFi() {
    WiFiManager wm;

    // UI Loading sebelum masuk portal
    showStatus("WIFI PORTAL", "Check your Phone!");

    // Jika dalam 3 menit tidak di-setting, dia akan stop portalnya
    wm.setConfigPortalTimeout(180);

    // Mencoba connect, kalau gagal dia buka AP "ESP32_Config_Portal"
    WiFi.mode(WIFI_AP_STA);
    delay(50);
    if (!wm.autoConnect("ESP32C3", "123456789")) {
        showStatus("FAILED", "Portal Timeout", true);
        delay(2000);
    } else {
        showStatus("SUCCESS", "WiFi Connected!");
        delay(1000);
    }
    startWiFiConnect();
}

// --- SAVED WIFI MENU ---

void drawSavedWiFiUI() {
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(5, 10, "SAVED NETWORKS");
    display.drawHLine(0, 13, 128);

    display.setFont(u8g2_font_7x14_tr);
    for (int i = 0; i < MAX_SAVED; i++) {
        int yPos = WC_START_Y + (i * (WC_ITEM_HEIGHT + 2));
        if (wifiMenuIndex == i) {
            display.setDrawColor(1);
            display.drawRBox(0, yPos, 128, WC_ITEM_HEIGHT, 3);
            display.setDrawColor(0);
        } else {
            display.setDrawColor(1);
        }
        display.drawStr(8, yPos + WC_TEXT_OFFSET - 1, savedSSIDs[i].c_str());
    }
    display.sendBuffer();
}

void onUpClick_Saved_WC() {
    wifiMenuIndex = (wifiMenuIndex - 1 + MAX_SAVED) % MAX_SAVED;
    drawSavedWiFiUI();
}

void onDownClick_Saved_WC() {
    wifiMenuIndex = (wifiMenuIndex + 1) % MAX_SAVED;
    drawSavedWiFiUI();
}

void onOK_Saved_WC() {
    showStatus("CONNECTING", savedSSIDs[wifiMenuIndex]);
    WiFi.begin(savedSSIDs[wifiMenuIndex].c_str(), savedPass[wifiMenuIndex].c_str());

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 40) {
        showConnectingUI("Saved Link");
        delay(100);
        retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        saveWiFi(savedSSIDs[wifiMenuIndex].c_str(), savedPass[wifiMenuIndex].c_str());
        showStatus("CONNECTED", "Profile Active");
    } else {
        showStatus("FAILED", "Out of Range", true);
    }
    delay(1000);
    startWiFiConnect();
}

// --- MAIN WIFI CONNECT UI ---

void drawWiFiConnectUI() {
    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(5, 10, "WIFI SETTINGS");
    display.drawHLine(0, 13, 128);

    display.setFont(u8g2_font_7x14_tr);

    // Item 0: Setup/Auto
    String msg = loadWiFi(ssid, pass) ? "Auto Connect" : "Manual Setup";
    if (wifiMenuIndex == 0) {
        display.setDrawColor(1);
        display.drawRBox(0, 18, 128, WC_ITEM_HEIGHT, 3);
        display.setDrawColor(0);
    } else {
        display.setDrawColor(1);
    }
    display.drawStr(8, 18 + WC_TEXT_OFFSET - 1, msg.c_str());

    // Item 1: Saved
    if (wifiMenuIndex == 1) {
        display.setDrawColor(1);
        display.drawRBox(0, 36, 128, WC_ITEM_HEIGHT, 3);
        display.setDrawColor(0);
    } else {
        display.setDrawColor(1);
    }
    display.drawStr(8, 36 + WC_TEXT_OFFSET - 1, "Saved Networks");

    // Hint
    display.setDrawColor(1);
    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(5, 62, "HOLD OK TO RETURN");

    display.sendBuffer();
}

void onUpClick_WC() {
    wifiMenuIndex = (wifiMenuIndex - 1 + 2) % 2;
    drawWiFiConnectUI();
}

void onDownClick_WC() {
    wifiMenuIndex = (wifiMenuIndex + 1) % 2;
    drawWiFiConnectUI();
}

void onOK_WC() {
    if (wifiMenuIndex == 0)
        ConnectToWiFi();
    else {
        wifiMenuIndex = 0;  // reset index untuk menu saved
        detachCallback();
        drawSavedWiFiUI();
        btnUp.attachClick(onUpClick_Saved_WC);
        btnDown.attachClick(onDownClick_Saved_WC);
        btnOK.attachClick(onOK_Saved_WC);
    }
}

// --- EXTERNAL FUNCTION ---

void WiFiConnect_Exit() {
}

void startWiFiConnect() {
    wifiMenuIndex = 0;
    isWiFiConnectRunning = true;

    detachCallback();
    drawWiFiConnectUI();

    btnUp.attachClick(onUpClick_WC);
    btnDown.attachClick(onDownClick_WC);
    btnOK.attachClick(onOK_WC);
    btnOK.attachLongPressStart([]() {
        isWiFiConnectRunning = false;
    });

    while (isWiFiConnectRunning) {
        btnUp.tick();
        btnDown.tick();
        btnOK.tick();

        drawWiFiConnectUI();
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }

    drawMenu();
    return;
}