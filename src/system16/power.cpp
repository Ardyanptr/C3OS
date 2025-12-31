#include "power.h"

#include <Arduino.h>
#include <esp_sleep.h>

static PowerState current = PowerState::ACTIVE;
static uint32_t lastActive = 0;

void powerNotifyActivity() {
    lastActive = millis();
    current = PowerState::ACTIVE;
}

void powerTick() {
    uint32_t idle = millis() - lastActive;

    if (idle > 30000 && current != PowerState::LIGHT_SLEEP) {
        current = PowerState::LIGHT_SLEEP;
        esp_light_sleep_start();
    }
}

bool isPowerLikely() {
    return ESP.getFreeHeap() < 20 * 1024;
}