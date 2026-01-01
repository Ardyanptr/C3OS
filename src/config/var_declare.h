#pragma once
#include <Arduino.h>

#include "config/config.h"
#include "config/var_declare.h"
#include "include/BLEAppLib.h"
#include "include/ComponentLib.h"
#include "include/CoreLib.h"
#include "include/EssentialAppLib.h"
#include "include/SystemCoreLib.h"
#include "include/UILib.h"
#include "include/WiFiAppLib.h"

String BOARD_ATTACHED = "ESP32-C3";
int BOARD_REV = 4;

U8G2_SSD1315_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

OneButton btnUp(BUTTON_UP, true);
OneButton btnDown(BUTTON_DOWN, true);
OneButton btnOK(BUTTON_OK, true);
OneButton btnAction(BUTTON_ACTION, true);

AT24C256 eep(0x50);

Settings setting;
PowerManager gPower;
DFRobot_VL53L0X sensor;