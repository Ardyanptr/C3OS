#include "update.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>

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
    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(48, 26, "Updating");

    display.drawStr(12, 39, "Keep the machine turned on");
    display.drawStr(8, 46, "Do not restart or power off!");

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

    btnOK.attachClick(drawMenu);
    btnOK.attachLongPressStart(proceedUpdate);
}

void updateHomepage() {
    display.clearBuffer();
    display.drawStr(2, 10, "Checking Updates...");
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
        manifestURL = localDoc["updates_config"]["url"].as<String>();

        localFile.close();
    } else {
        localVersion = "0.0.0";
        manifestURL = "https://raw.githubusercontent.com/Ardyanptr/C3OS/refs/heads/main/data/cfg/update.json";
    }

    http.begin(client, manifestURL);
    int httpcode = http.GET();

    if (httpcode == HTTP_CODE_OK) {
        String payload = http.getString();
        DynamicJsonDocument remoteDoc(2048);
        DeserializationError error = deserializeJson(remoteDoc, payload);

        if (error) {
            Serial.println("Failed to parse remote JSON");
            updateWiFiError();
            return;
        }

        String remoteVersion = remoteDoc["ver"].as<String>();

        if (remoteVersion == localVersion) {
            display.clearBuffer();
            display.drawStr(2, 30, "System is Up-To-Date");
            display.sendBuffer();

            delay(2000);

            return;
        }

        display.clearBuffer();
        display.printf("New Update: %s", remoteVersion.c_str());
        display.sendBuffer();

        if (remoteDoc.containsKey("updates_file")) {
            const char* path = remoteDoc["updates_file"]["local_path"];
            const char* url = remoteDoc["updates_file"]["url"];

            updateUpdatingScreen();

            if (downloadAndSave(client, url, path)) {
                Serial.println("File Update Success");
            }
        }

        if (remoteDoc.containsKey("updates_config")) {
            const char* path = remoteDoc["updates_config"]["local_path"];
            const char* url = remoteDoc["updates_config"]["url"];

            updateUpdatingScreen();

            if (downloadAndSave(client, url, path)) {
                Serial.println("File Update Success");

                display.clearBuffer();
                display.setFontMode(1);
                display.setBitmapMode(1);
                display.setFont(u8g2_font_4x6_tr);

                display.drawStr(34, 30, "Update Complete");
                display.drawStr(36, 37, "Restarting... ");

                display.sendBuffer();

                delay(2000);
                ESP.restart();
            }
        }
    }
}

// Initial Running
void proceedUpdate() {
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
        return;
    } else {
        updateHomepage();
        return;
    }
}
