#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include <OneButton.h>

#include "icons/icon.h"
#include "component/button.h"
#include "system16/esp826.h"
#include "system16/ui_manager.h"

#include "config/config.h"

extern void drawMenu();

void runEchoSniffer();