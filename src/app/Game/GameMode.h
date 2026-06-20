#pragma once

#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>
#include <WiFi.h>

#include "app/Essential/Settings.h"
#include "component/button.h"
#include "icons/icon.h"

#include "config/config.h"

extern void drawMenu();

void runGameMode();