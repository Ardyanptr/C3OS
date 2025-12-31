#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "component/panic.h"

namespace MEM {

struct MonitorConfig {
    size_t minHeapThreshold = 30 * 1024;
    UBaseType_t minStackThreshold = 100;
};

enum class Event {
    OK,
    HEAP_LOW,
    HEAP_CRITICAL,
    STACK_LOW,
    HEAP_LEAK
};

inline MonitorConfig config;

inline Event checkTask(const char* taskName = nullptr) {
    size_t heap = ESP.getFreeHeap();
    if (heap < config.minHeapThreshold / 2) {
        return Event::HEAP_CRITICAL;
    }

    if (heap < config.minHeapThreshold) {
        return Event::HEAP_LOW;
    }

    if (uxTaskGetStackHighWaterMark(NULL) < config.minStackThreshold)
        return Event::STACK_LOW;

    return Event::OK;
}

inline Event checkHeapTrend() {
    static size_t last = ESP.getFreeHeap();
    size_t now = ESP.getFreeHeap();

    if (now < last && last - now > 2048) {
        last = now;
        return Event::HEAP_LEAK;
    }

    last = now;
    return Event::OK;
}

inline void* mallocSafe(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) {
        Serial.printf("[MEM ERROR] malloc fail: %u byytes\n", size);
    }

    return ptr;
}

inline void freeSafe(void* ptr) {
    if (ptr) {
        free(ptr);
        ptr = nullptr;
    }
}

inline void monitorLoop(const char* taskName = nullptr) {
    static uint32_t lastCheck = 0;
    uint32_t now = millis();

    if (now - lastCheck > 5000) {
        lastCheck = now;
        checkTask(taskName);
    }
}

inline UBaseType_t stackLeft(TaskHandle_t h) {
    return h ? uxTaskGetStackHighWaterMark(h) : 0;
}

inline bool isCurrentTaskStackLow() {
    return uxTaskGetStackHighWaterMark(NULL) < config.minStackThreshold;
}

}  // namespace MEM

namespace SYS {

inline void handleMemEvent(MEM::Event ev, const char* svc = nullptr) {
    switch (ev) {
        case MEM::Event::STACK_LOW:
            stopService(svc);
            break;
        case MEM::Event::HEAP_LOW:
            stopAllService();
            break;
        case MEM::Event::HEAP_LEAK:
            stopAllService();
            break;
        case MEM::Event::HEAP_CRITICAL:
            panic(PANIC_MEM_CORRUPT, "Out Of Memory");
            break;
        default:
            break;
    }
}

}  // namespace SYS