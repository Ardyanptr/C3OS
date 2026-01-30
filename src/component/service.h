#pragma once
#include <Arduino.h>

enum class ServicePriority : uint8_t {
    PRIO_LOW = 1,
    PRIO_NORMAL = 2,
    PRIO_HIGH = 3,
    PRIO_CRITICAL = 4
};

typedef void (*ServiceFunc)(void*);

struct ServiceInfo {
    const char* name;
    bool running;
    size_t stackUsed;  // PASTIKAN TULISANNYA stackUsed
    uint32_t lastHeartbeat;
};

// Pastikan semua fungsi ini ada di sini agar file lain bisa melihatnya
int getServiceCount();
ServiceInfo getServiceInfo(int index);
bool serviceStopRequested(const char* name);  // Tadi ini mungkin belum ada di header

void registerService(const char* name, ServiceFunc fn, uint16_t stack = 2048, ServicePriority prio = ServicePriority::PRIO_NORMAL);
void startService();
void stopAllService();
void updateServiceHeartbeat(const char* name);