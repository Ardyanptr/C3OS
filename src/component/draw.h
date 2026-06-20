#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

#include "config/config.h"

void drawAutoScrollText(const String& text, int startY);
