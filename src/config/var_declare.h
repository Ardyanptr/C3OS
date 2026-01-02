#pragma once
#include <Arduino.h>
#include <DFRobot_VL53L0X.h>
#include <OneButton.h>
#include <U8g2lib.h>
#include <app/Essential/Settings.h>
#include <system16/AT24C256_EEPROM.h>
#include <system16/power_manager.h>

extern String BOARD_ATTACHED;
extern int BOARD_REV;

extern U8G2_SSD1315_128X64_NONAME_F_HW_I2C display;

extern OneButton bntUp, btnDown, btnOK, btnAction;
extern AT24C256 eep;

extern Settings setting;
extern PowerManager gPower;
extern DFRobot_VL53L0X sensor;