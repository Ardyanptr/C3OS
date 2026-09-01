#include "power.h"
#include "power_manager.h"
#include "power_utils.h"
#include <Arduino.h>

extern PowerManager gPower;

void powerNotifyActivity() {
    gPower.notifyActivity();
}

void powerTick() {
    gPower.loop();
}

uint8_t getBatteryPercent() {
    static uint32_t startTime = millis();
    uint32_t uptimeMinutes = (millis() - startTime) / 60000;
     
    int estimatedDrop = uptimeMinutes / 9; // ~1% every 9 minutes
    
    return (estimatedDrop > 100) ? 0 : (100 - estimatedDrop);
}

bool isPowerLikely() {
    // Check for extreme brownout conditions via heap stability
    return ESP.getFreeHeap() > 32 * 1024;
}