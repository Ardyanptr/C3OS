#include "loader.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

Config sysConfig;

bool loadConfig() {
    File configFile_UI = LittleFS.open("/cfg/ui_layout.json", "r");
    if (!configFile_UI) {
        Serial.println("Failed to open config file");
        return false;
    }

    StaticJsonDocument<512> doc;

    DeserializationError error = deserializeJson(doc, configFile_UI);
    if (error) {
        Serial.println("Failed to read config file");
        return false;
    }

    Serial.println("Loading config file");
    sysConfig.text_offset_y = doc["ui"]["text_offset_y"];
    sysConfig.draw_count = doc["ui"]["draw_count"];
    sysConfig.item_height = doc["ui"]["item_height"];
    sysConfig.scroll_duration = doc["ui"]["scroll_duration"];
    sysConfig.visible_items = doc["ui"]["visible_items"];

    sysConfig.anr_active = doc["system"]["anr_active"];
    sysConfig.system_ui_active = doc["system"]["system_ui_active"];
    configFile_UI.close();

    File configFile_System = LittleFS.open("/cfg/system_config.json", "r");
    if (!configFile_System) {
        Serial.println("Failed to open config file");
        return false;
    }

    StaticJsonDocument<512> doc2;

    DeserializationError error_doc2 = deserializeJson(doc2, configFile_System);
    if (error_doc2) {
        Serial.println("Failed to read config file");
        return false;
    }

    sysConfig.init_tof = doc2["boot"]["init_tof"];
    sysConfig.init_eeprom = doc2["boot"]["init_eeprom"];
    sysConfig.init_service = doc2["boot"]["init_service"];
    sysConfig.ignore_hardware = doc2["boot"]["ignore_hardware"];
    sysConfig.no_init = doc2["boot"]["no_init"];

    // for user pin
    if(!doc2["user_pin"].isNull()) {
        JsonObject user_pin = doc2["user_pin"];

        for(int i=0;i<4;i++) {
            JsonObject pin = doc2["user_pin"];

            for(int i=0;i<4;i++) {
                sysConfig.user_pin[i] = pin[String("pin") + (i + 1)] | 0;
            }
        }
    }
    configFile_System.close();

    Serial.println("Config Loaded Successfully");
    return true;
}

bool saveConfig() {
    // 1. Save UI layout config
    File configFile_UI = LittleFS.open("/cfg/ui_layout.json", "w");
    if (!configFile_UI) {
        Serial.println("Failed to open UI config for writing");
        return false;
    }

    StaticJsonDocument<512> doc;
    JsonObject uiObj = doc.createNestedObject("ui");
    uiObj["text_offset_y"] = sysConfig.text_offset_y;
    uiObj["draw_count"] = sysConfig.draw_count;
    uiObj["item_height"] = sysConfig.item_height;
    uiObj["scroll_duration"] = sysConfig.scroll_duration;
    uiObj["visible_items"] = sysConfig.visible_items;

    JsonObject sysObj = doc.createNestedObject("system");
    sysObj["anr_active"] = sysConfig.anr_active;
    sysObj["system_ui_active"] = sysConfig.system_ui_active;

    if (serializeJson(doc, configFile_UI) == 0) {
        Serial.println("Failed to serialize UI config json");
    }
    configFile_UI.close();

    // 2. Save System config
    File configFile_System = LittleFS.open("/cfg/system_config.json", "w");
    if (!configFile_System) {
        Serial.println("Failed to open System config for writing");
        return false;
    }

    StaticJsonDocument<512> doc2;
    JsonObject bootObj = doc2.createNestedObject("boot");
    bootObj["init_tof"] = sysConfig.init_tof;
    bootObj["init_eeprom"] = sysConfig.init_eeprom;
    bootObj["init_service"] = sysConfig.init_service;
    bootObj["ignore_hardware"] = sysConfig.ignore_hardware;
    bootObj["no_init"] = sysConfig.no_init;

    JsonObject pinObj = doc2.createNestedObject("user_pin");
    pinObj["pin1"] = sysConfig.user_pin[0];
    pinObj["pin2"] = sysConfig.user_pin[1];
    pinObj["pin3"] = sysConfig.user_pin[2];
    pinObj["pin4"] = sysConfig.user_pin[3];

    if (serializeJson(doc2, configFile_System) == 0) {
        Serial.println("Failed to serialize System config json");
    }
    configFile_System.close();

    Serial.println("Config Saved Successfully");
    return true;
}
