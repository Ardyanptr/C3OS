#pragma once
#include <Arduino.h>

inline float easeOut(float t) {
    return 1 - pow(1 - t, 2);
}

inline float easeOutCubic(float t) {
    return 1 - pow(1 - t, 3);
}

inline float easeInOutCubic(float t) {
    return t < 0.5 ? 4 * t * t * t : 1 - pow(-2 * t + 2, 3) / 2;
}