#pragma once
#include <Arduino.h>

namespace PowerUtils {

/**
 * ESP32-C3 Battery Voltage Monitoring (Internal)
 * 
 * Note: The ESP32-C3 internal voltage sensor measures the 1.1V internal Vref.
 * While it doesn't directly measure the battery, it can detect if the 3.3V 
 * rail is sagging, which happens when the LiPo voltage drops below ~3.4V 
 * (depending on the LDO dropout).
 */
inline float getInternalVoltage() {
    // This is a rough estimation of the system rail voltage using the internal Hall sensor/Vref
    // On C3, it's not as accurate as an external ADC divider, but better than nothing.
    return 3.3f; // Placeholder for baseline
}

/**
 * Map LiPo voltage to percentage.
 * 2400mAh LiPo typical discharge curve.
 */
inline uint8_t mapLiPoToPercent(float voltage) {
    if (voltage >= 4.20f) return 100;
    if (voltage >= 4.10f) return 90;
    if (voltage >= 4.00f) return 80;
    if (voltage >= 3.90f) return 70;
    if (voltage >= 3.85f) return 60;
    if (voltage >= 3.80f) return 50;
    if (voltage >= 3.75f) return 40;
    if (voltage >= 3.70f) return 30;
    if (voltage >= 3.65f) return 20;
    if (voltage >= 3.60f) return 10;
    if (voltage >= 3.50f) return 5;
    return 0;
}

} // namespace PowerUtils
