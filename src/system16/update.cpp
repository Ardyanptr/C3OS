#include "update.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>

#include "icons/icon.h"

DynamicJsonDocument remoteDoc(2048);

bool updater_Running = true;

// Misc
void drawOrnament() {
    display.drawFilledEllipse(0, 1, 13, 13);
    display.drawBox(119, 56, 16, 16);
    display.drawFrame(113, 55, 4, 4);
    display.drawBox(111, 47, 5, 5);
    display.drawFrame(119, 48, 5, 5);
    display.drawLine(108, -1, 131, 14);
    display.drawLine(-2, 49, 15, 65);
}

void drawOrnament_2() {
    display.drawFrame(6, 21, 5, 5);
    display.drawEllipse(29, 23, 17, 2);
    display.drawFrame(48, 21, 12, 5);
    display.drawLine(61, 23, 118, 23);
    display.drawFrame(114, 39, 5, 5);
    display.drawEllipse(95, 41, 17, 2);
    display.drawFrame(65, 39, 12, 5);
    display.drawLine(6, 41, 63, 41);
}

// Main
bool downloadAndSave(WiFiClientSecure& client, String url, String path) {
    HTTPClient http;

    if (!http.begin(client, url)) {
        Serial.println("HTTP Begin Failed");
        return false;
    }

    http.setTimeout(10000);

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        int totalSize = http.getSize();

        if (totalSize <= 0) {
            Serial.println("Error: File is empty nor size unknown");
            return false;
        }

        File f = LittleFS.open(path, "w");
        if (!f) {
            Serial.println("Error:LFS: Failed to open file for writing " + path);
            return false;
        }

        Serial.printf("Downloading %s (%d bytes)...\n", path.c_str(), totalSize);

        size_t written = http.writeToStream(&f);

        f.close();
        http.end();

        if (written > 0 && (int)written == totalSize) {
            return true;
        } else {
            Serial.println("Write mismatch or failed");
            return false;
        }
    } else {
        Serial.printf("HTTP GET Failed, error: %s\n", http.errorToString(httpCode).c_str());
        http.end();

        return false;
    }
}

void updateUpdatingScreen() {
    display.clearBuffer();

    display.setFontMode(1);
    display.setBitmapMode(1);

    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(4, 31, "Getting Things Ready");

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(16, 40, "Preparing for an Updates");

    drawOrnament();

    display.sendBuffer();
}

void updateWiFiError() {
    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);
    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(2, 10, "WiFi Error");

    display.setFont(u8g2_font_5x7_tr);
    display.drawStr(2, 20, "Try connecting to WiFi");
    display.drawStr(2, 28, "or Try again later!");

    display.drawRFrame(1, 53, 28, 9, 3);

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(11, 60, "OK");

    display.drawRFrame(31, 53, 28, 9, 3);
    display.drawStr(35, 60, "Retry");
    display.sendBuffer();

    btnOK.attachClick([]() {
        updater_Running = false;
    });
    btnOK.attachLongPressStart(proceedUpdate);
}

// Update Installation

void update_installUpdate() {
    WiFiClientSecure client;
    client.setInsecure();

    if (remoteDoc.containsKey("updates_file")) {
        const char* path = remoteDoc["updates_file"]["local_path"];
        const char* url = remoteDoc["updates_file"]["url"];

        updateUpdatingScreen();

        if (downloadAndSave(client, url, path)) {
            Serial.println("File Update Success");
        }
    }

    if (remoteDoc.containsKey("updates_config")) {
        const char* path = "/cfg/update.json";
        const char* url = remoteDoc["updates_config"]["url"];

        updateUpdatingScreen();

        if (downloadAndSave(client, url, path)) {
            Serial.println("File Update Success");
            display.clearBuffer();

            display.setFontMode(1);
            display.setBitmapMode(1);

            display.setFont(u8g2_font_6x10_tr);
            display.drawStr(22, 31, "Cleaning Up...");

            display.setFont(u8g2_font_4x6_tr);
            display.drawStr(12, 40, "Update Done, Restarting...");

            drawOrnament();

            display.sendBuffer();

            remoteDoc.clear();
            delay(2000);

            esp_restart();
        }
    }
}

void updateHomepage() {
    display.clearBuffer();

    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(7, 36, "Checking Updates...");

    drawOrnament_2();

    display.sendBuffer();

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    String localVersion = "";
    String manifestURL = "";

    File localFile = LittleFS.open("/cfg/update.json", "r");
    if (localFile) {
        StaticJsonDocument<512> localDoc;
        deserializeJson(localDoc, localFile);

        localVersion = localDoc["ver"].as<String>();
        manifestURL = "https://raw.githubusercontent.com/Ardyanptr/C3OS/refs/heads/main/data/cfg/update.json";

        localFile.close();
    } else {
        localVersion = "0.0.0";
        manifestURL = "https://raw.githubusercontent.com/Ardyanptr/C3OS/refs/heads/main/data/cfg/update.json";
    }

    http.begin(client, manifestURL);
    int httpcode = http.GET();

    if (httpcode == HTTP_CODE_OK) {
        String payload = http.getString();

        remoteDoc.clear();
        DeserializationError error = deserializeJson(remoteDoc, payload);

        if (error) {
            Serial.println("Failed to parse remote JSON");
            updateWiFiError();
            return;
        }

        String remoteVersion = remoteDoc["ver"].as<String>();

        if (remoteVersion == localVersion) {
            display.clearBuffer();
            display.setFontMode(1);
            display.setBitmapMode(1);

            display.setFont(u8g2_font_5x7_tr);
            display.drawStr(9, 10, "No Pending Update");

            display.drawXBM(2, 2, 5, 8, image_Attention_bits);

            display.setFont(u8g2_font_5x8_tr);
            display.drawStr(3, 21, "Your system is up-to-date");
            display.drawStr(3, 30, "No Action needed");

            display.setFont(u8g2_font_5x7_tr);
            display.drawRFrame(3, 52, 25, 10, 3);
            display.drawStr(6, 60, "Exit");

            display.drawRFrame(29, 52, 29, 10, 3);
            display.drawStr(31, 60, "Check");

            display.sendBuffer();

            btnOK.attachClick([]() {
                updater_Running = false;
            });

            btnOK.attachLongPressStart(proceedUpdate);
            return;
        }

        display.clearBuffer();
        display.setFontMode(1);
        display.setBitmapMode(1);
        display.setFont(u8g2_font_5x7_tr);
        display.drawStr(9, 10, "Pending Update");

        display.drawXBM(2, 2, 5, 8, image_Attention_bits);

        display.setFont(u8g2_font_5x8_tr);
        display.drawStr(3, 21, String("Version: " + remoteVersion).c_str());
        display.drawStr(3, 30, String("Current Version: " + localVersion).c_str());

        display.setFont(u8g2_font_5x7_tr);
        display.drawRFrame(3, 52, 39, 10, 3);
        display.drawStr(5, 60, "INSTALL");

        display.drawRFrame(43, 52, 25, 10, 3);
        display.drawStr(46, 60, "Exit");

        display.sendBuffer();

        btnOK.attachClick(update_installUpdate);
        btnOK.attachLongPressStart([]() {
            updater_Running = false;
        });
    }

    http.end();
}

// Initial Running
void proceedUpdate() {
    updater_Running = true;

    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(36, 32, "Please Wait...");
    display.drawStr(20, 38, "Connecting to Internet");

    display.sendBuffer();

    WiFiManager wm;
    wm.setConfigPortalTimeout(1);

    if (!wm.autoConnect("ESP32C3", "123456789")) {
        updateWiFiError();
    } else {
        updateHomepage();
    }

    while (updater_Running) {
        btnOK.tick();
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }

    drawMenu();
    return;
}
