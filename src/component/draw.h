#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

extern U8G2_SSD1315_128X64_NONAME_F_HW_I2C display;

void drawAutoScrollText(const String& text, int startY);
