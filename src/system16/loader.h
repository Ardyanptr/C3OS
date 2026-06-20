#pragma once

#include <Arduino.h>

struct Config {
    // ui_layout.json
    int text_offset_y;
    int draw_count;
    int item_height;
    int scroll_duration;
    int visible_items;
    bool anr_active;
    bool system_ui_active;

    // system_config.json
    bool init_tof;
    bool init_eeprom;
    bool init_service;
    bool ignore_hardware;
    bool no_init;

    uint8_t user_pin[4];
};

extern Config sysConfig;

bool loadConfig();
bool saveConfig();