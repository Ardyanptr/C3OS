#pragma once
#include <Arduino.h>

#include "include/all_.h"

extern uint32_t SLEEP_TIMEOUT;

extern bool isOnSleepMode;
extern bool pendingLockscreen;

extern TaskHandle_t esp8266TaskHandle;

void boot();