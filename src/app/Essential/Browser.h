#pragma once
#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>

#include "component/vkeyboard.h"

extern U8G2_SSD1315_128X64_NONAME_F_HW_I2C display;
extern OneButton btnUp;
extern OneButton btnDown;
extern OneButton btnOK;
extern OneButton btnAction;

extern void drawMenu();
extern void appHeartBeat();

// Launch the simple text browser. It requires WiFi to be connected.
void runBrowser();
