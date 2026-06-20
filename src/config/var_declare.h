#pragma once
#include <Arduino.h>
#include <DFRobot_VL53L0X.h>
#include <OneButton.h>
#include <U8g2lib.h>
#include <app/Essential/Settings.h>
#include <system16/AT24C256_EEPROM.h>
#include <system16/power_manager.h>
#include "config/config.h"
#include "component/process_manager.h"

extern String BOARD_ATTACHED;
extern int BOARD_REV;

extern AT24C256 eep;

extern int menuIndex;

extern Settings setting;
extern PowerManager gPower;
extern DFRobot_VL53L0X sensor;