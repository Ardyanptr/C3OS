#pragma once
#include <Arduino.h>
#include <WiFi.h>

#include "LittleFS.h"
#include "component/panic.h"
#include "config/config.h"

void runLuaScript(const char* path);