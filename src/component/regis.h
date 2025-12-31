#pragma once
#include <Arduino.h>
#include <Preferences.h>

void saveWiFi(const char* ssid, const char* pass);
bool loadWiFi(String &ssid, String &pass);
void clearWiFi();

void saveBLE(bool enabled);
bool loadBLE(bool &enabled);

void saveSleep(uint32_t timeout);
bool loadSleep(uint32_t &timeout);

void savePowerMode(uint8_t mode);
bool loadPowerMode(uint8_t &mode);