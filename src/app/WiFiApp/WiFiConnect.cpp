#include "WiFiConnect.h"
#include "esp_task_wdt.h"

#include <Arduino.h>
#include <WiFiManager.h>
#include <esp_task.h>
#include "component/wifiservice.h"

// UI Constants - Diselaraskan dengan Menu Utama
const int WC_ITEM_HEIGHT = 16;
const int WC_TEXT_OFFSET = 13;
const int WC_START_Y = 18;

// Menu State
#define MAX_SAVED 3

String savedSSIDs[MAX_SAVED] = {"_ARDASYAH", "ardyan", "ALRD-Ardyan"};
String savedPass[MAX_SAVED] = {"Satunusasatubangsa", "ardyann", "12345678"};

int wifiMenuIndex = 0;

String ssid, pass;

bool WiFiConnect_Running = true;

// --- UTILITY UI ---

void exitWiFiConnect() {
    WiFiConnect_Running = false;
    return;
}

void showStatus(String title, String msg, bool isError = false) {
    display.clearBuffer();
    display.setFont(u8g2_font_7x14_tr);
    display.setDrawColor(1);
    display.drawStr(15, 28, title.c_str());

    display.setFont(u8g2_font_5x7_tr);
    display.drawStr(15, 42, msg.c_str());

    if (isError) display.drawFrame(0, 0, 128, 64); // Frame tambahan kalau error
    display.sendBuffer();
}

void showConnectingUI(String label) {
    static int dotCount = 0;
    dotCount = (dotCount + 1) % 4;
    String dots = label;
    for (int i = 0; i < dotCount; i++)
        dots += ".";

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

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    mac[5] ^= esp_random() & 0xFF;
    esp_wifi_set_mac(WIFI_IF_STA, mac);

    delay(50);

    esp_task_wdt_reset();
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

    if(savedSSIDs[wifiMenuIndex] == "ALRD-Ardyan") {
        IPAddress local_IP(192,168,4,2);
        IPAddress gateway(192,168,4,1);
        IPAddress subnet(255,255,255,0);
        WiFi.config(local_IP, gateway, subnet);
    } else {
        WiFi.config(IPAddress(0,0,0,0), IPAddress(0,0,0,0), IPAddress(0,0,0,0));
    }

    WiFi.begin(savedSSIDs[wifiMenuIndex].c_str(), savedPass[wifiMenuIndex].c_str());

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 40) {
        esp_task_wdt_reset();

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

    // Item 2: Status
    if (wifiMenuIndex == 2) {
        display.setDrawColor(1);
        display.drawRBox(0, 54, 128, WC_ITEM_HEIGHT, 3);
        display.setDrawColor(0);
    } else {
        display.setDrawColor(1);
    }
    display.drawStr(8, 54 + WC_TEXT_OFFSET - 1, "Network Status");

    display.sendBuffer();
}

void onUpClick_WC() {
    wifiMenuIndex = (wifiMenuIndex - 1 + 3) % 3;
    drawWiFiConnectUI();
}

void onDownClick_WC() {
    wifiMenuIndex = (wifiMenuIndex + 1) % 3;
    drawWiFiConnectUI();
}

void drawDetailedStatus() {
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tr);
    display.setDrawColor(1);
    display.drawStr(5, 10, "NETWORK STATUS");
    display.drawHLine(0, 13, 128);

    display.setFont(u8g2_font_4x6_tr);
    if (WiFi.status() == WL_CONNECTED) {
        char buf[64];
        snprintf(buf, sizeof(buf), "SSID: %s", WiFi.SSID().c_str());
        display.drawStr(5, 22, buf);
        
        snprintf(buf, sizeof(buf), "IP: %s", WiFi.localIP().toString().c_str());
        display.drawStr(5, 30, buf);
        
        snprintf(buf, sizeof(buf), "RSSI: %d dBm", WiFi.RSSI());
        display.drawStr(5, 38, buf);
        
        snprintf(buf, sizeof(buf), "DL: %.1f KB/s", g_wifiStats.downloadSpeed);
        display.drawStr(5, 46, buf);
        
        snprintf(buf, sizeof(buf), "UL: %.1f KB/s", g_wifiStats.uploadSpeed);
        display.drawStr(5, 54, buf);
        
        uint32_t total = (g_wifiStats.totalRX + g_wifiStats.totalTX) / 1024;
        snprintf(buf, sizeof(buf), "Used: %u KB", total);
        display.drawStr(5, 62, buf);
    } else {
        display.drawStr(5, 35, "Disconnected");
    }
    display.sendBuffer();
}

void onOK_WC() {
    if (wifiMenuIndex == 0) {
        ConnectToWiFi();
    } else if (wifiMenuIndex == 1) {
        wifiMenuIndex = 0; // reset index untuk menu saved
        detachCallback();
        drawSavedWiFiUI();
        btnUp.attachClick(onUpClick_Saved_WC);
        btnDown.attachClick(onDownClick_Saved_WC);
        btnOK.attachClick(onOK_Saved_WC);
    } else if (wifiMenuIndex == 2) {
        // Simple loop for status screen
        static bool exitStatus;
        exitStatus = false;
        btnOK.attachClick([](){ exitStatus = true; });
        
        while (!exitStatus) {
            drawDetailedStatus();
            btnUp.tick();
            btnDown.tick();
            btnOK.tick();
            vTaskDelay(100 / portTICK_PERIOD_MS);
            esp_task_wdt_reset();
            if (WiFiConnect_Running == false) break;
        }
        
        // Restore callbacks
        btnUp.attachClick(onUpClick_WC);
        btnDown.attachClick(onDownClick_WC);
        btnOK.attachClick(onOK_WC);
        drawWiFiConnectUI();
    }
}

// --- EXTERNAL FUNCTION ---

void startWiFiConnect() {
    WiFiConnect_Running = true;

    wifiMenuIndex = 0;
    detachCallback();
    drawWiFiConnectUI();

    btnUp.attachClick(onUpClick_WC);
    btnDown.attachClick(onDownClick_WC);
    btnOK.attachClick(onOK_WC);
    btnOK.attachLongPressStart(exitWiFiConnect);

    while (WiFiConnect_Running) {
        btnUp.tick();
        btnDown.tick();
        btnOK.tick();

        vTaskDelay(5 / portTICK_PERIOD_MS);
        esp_task_wdt_reset();
    }

    drawMenu();
    return;
}
