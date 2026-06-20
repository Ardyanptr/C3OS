#pragma once

#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>

#include "config/config.h"

extern void drawMenu();

void proceedUpdate();
void applyPendingUpdate();