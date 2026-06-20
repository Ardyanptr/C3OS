#pragma once
#include <Arduino.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"

enum AppCaps {
    CAP_WIFI = 1 << 0,
    CAP_BT = 1 << 1,
    CAP_UI = 1 << 2,
    CAP_IR = 1 << 3,
};

struct AppDesc {
    const char* name;
    void (*entry)();
    uint32_t caps;
    uint32_t minHeap;
    bool allowSleep;
};

struct HeapInfo {
    const char* name;
    size_t heapUsed;
    UBaseType_t stackUsed;
    bool running;
};

extern AppDesc appTable[];
extern const int APP_COUNT;