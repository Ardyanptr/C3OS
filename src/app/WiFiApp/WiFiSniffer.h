#pragma once

#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>

#include "component/button.h"
#include "icons/icon.h"

#include "config/config.h"

extern void drawMenu();
extern void appHeartBeat();

void startSniffing();