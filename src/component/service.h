#pragma once
#include <Arduino.h>

typedef void (*ServiceFunc)(void*);

struct ServiceInfo {
    const char* name;
    bool running;
    size_t stakcUsed;
};

int getServiceCount();
ServiceInfo getServiceInfo(int index);

void registerService(const char* name, ServiceFunc fn, uint16_t stack = 2048);
void startService();
void stopAllService();
void stopService(const char* name);
bool serviceStopRequested(const char* name);
