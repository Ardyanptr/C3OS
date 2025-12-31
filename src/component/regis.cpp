#include <Arduino.h>
#include "regis.h"

Preferences prefs;

void saveWiFi(const char* ssid, const char* pass) {
    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();
}

bool loadWiFi(String &ssid, String &pass) {
    prefs.begin("wifi", true);
    if(!prefs.isKey("ssid") || !prefs.isKey("pass")) {
        prefs.end();
        return false;
    }

    ssid = prefs.getString("ssid");
    pass = prefs.getString("pass");
    
    prefs.end();
    return true;
}

void clearWiFi() {
    prefs.begin("wifi", false);
    prefs.clear();
    prefs.end();
}

void saveBLE(bool enabled) {
    prefs.begin("ble", false);
    prefs.putBool("enabled", enabled);
    prefs.end();
}

bool loadBLE(bool &enabled) {
    prefs.begin("ble", true);

    if(!prefs.isKey("enabled")) {
        prefs.end();
        return false;
    }

    enabled = prefs.getBool("enabled");

    prefs.end();
    return true;
}

void saveSleep(uint32_t timeout) {
    prefs.begin("sleep", false);
    prefs.putUShort("timeout", timeout);
    prefs.end();
}

bool loadSleep(uint32_t &timeout) {
    prefs.begin("sleep", true);

    if(!prefs.isKey("timeout")) {
        prefs.end();
        return false;
    }

    timeout = prefs.getUShort("timeout");

    prefs.end();
    return true;
}

void savePowerMode(uint8_t mode) {
    prefs.begin("power", false);
    prefs.putUChar("mode", mode);
    prefs.end();
}

bool loadPowerMode(uint8_t &mode) {
    prefs.begin("power", true);

    if(!prefs.isKey("mode")) {
        prefs.end();
        return false;
    }

    mode = prefs.getUChar("mode");

    prefs.end();
    return true;
}