#pragma once
#include <Arduino.h>

inline float easeOut(float t) { return 1 - pow(1 - t, 2); }

inline float easeOutCubic(float t) { return 1 - pow(1 - t, 3); }

inline float easeInOutCubic(float t) {
    return t < 0.5 ? 4 * t * t * t : 1 - pow(-2 * t + 2, 3) / 2;
}

static inline float easeOutExpo(float t) {
    return t >= 1.0f ? 1.0f : 1.0f - powf(2.0f, -10.0f * t);
}
static inline float easeInExpo(float t) { return t <= 0.0f ? 0.0f : powf(2.0f, 10.0f * t - 10.0f); }

inline float easeOutBack(float t) {
    const float c1 = 1.70158;
    const float c3 = c1 + 1;
    return 1 + c3 * pow(t - 1, 3) + c1 * pow(t - 1, 2);
}

inline float lerp(float a, float b, float f) {
    return a + f * (b - a);
}