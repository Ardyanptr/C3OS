#pragma once
#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>

#include "component/button.h"
#include "icons/icon.h"
#include "system16/esp826.h"

#include "config/config.h"

extern void drawMenu();

void draw_waitESP8266Close();
void draw_restarting();
void draw_restartESP8266();

void draw_Button(int pos, int count);