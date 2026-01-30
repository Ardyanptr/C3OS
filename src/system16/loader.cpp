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
    configFile_System.close();

    Serial.println("Config Loaded Successfully");
    return true;
}
