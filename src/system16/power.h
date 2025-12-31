#pragma once
#include <Arduino.h>

enum class PowerState {
    ACTIVE,
    IDLE,
    LIGHT_SLEEP,
    DEEP_SLEEP
};

void powerNotifyActivity();
void powerTick();

bool isPowerLikely();