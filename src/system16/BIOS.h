#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

#include "config/config.h"

void runBIOS_SAFE_MODE();
void runBIOS();
void runLuaScript(const char* path);

extern bool upFlag_BIOS;
extern bool downFlag_BIOS;
extern bool okFlag_BIOS;
extern bool actionFlag_BIOS;
extern bool okLong_BIOS;