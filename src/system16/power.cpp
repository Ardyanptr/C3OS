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

/**
 * Returns a simulated battery percentage based on uptime and heap stability
 * since a hardware ADC divider is not present.
 */
uint8_t getBatteryPercent() {
    static uint32_t startTime = millis();
    uint32_t uptimeMinutes = (millis() - startTime) / 60000;
    
    // Estimate: 2400mAh LiPo on C3 should last ~15-20 hours of active use
    // 15 hours = 900 minutes. 
    int estimatedDrop = uptimeMinutes / 9; // ~1% every 9 minutes
    
    return (estimatedDrop > 100) ? 0 : (100 - estimatedDrop);
}

bool isPowerLikely() {
    // Check for extreme brownout conditions via heap stability
    return ESP.getFreeHeap() > 32 * 1024;
}