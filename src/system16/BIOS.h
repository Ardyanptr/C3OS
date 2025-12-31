#pragma once
#include <Arduino.h>

#include "config/config.h"
#include "system16/AT24C256_EEPROM.h"
#include "system16/state.h"

extern U8G2_SSD1315_128X64_NONAME_F_HW_I2C display;
// Tambahkan deklarasi helper jika diperlukan
void runBIOS_SAFE_MODE();
void runBIOS();

// Flags sensor/button
extern bool upFlag_BIOS;
extern bool downFlag_BIOS;
extern bool okFlag_BIOS;
extern bool actionFlag_BIOS;
extern bool okLong_BIOS;