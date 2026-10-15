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
    size_t stackUsed;
    uint32_t lastHeartbeat;
};

int getServiceCount();
ServiceInfo getServiceInfo(int index);
bool serviceStopRequested(const char* name);

bool stopService(const char* name);
bool startServiceByName(const char* name);

void registerService(const char* name, ServiceFunc fn, uint16_t stack = 2048, ServicePriority prio = ServicePriority::PRIO_NORMAL);
void startService();
void stopAllService();
void updateServiceHeartbeat(const char* name);