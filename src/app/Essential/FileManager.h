#pragma once

#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>

extern U8G2_SSD1315_128X64_NONAME_F_HW_I2C display;
extern OneButton btnUp, btnDown, btnOK, btnAction;
extern void drawMenu();

void runFileManager();