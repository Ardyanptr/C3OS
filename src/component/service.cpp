#include "service.h"

#include <Arduino.h>
#include <string.h>

#include "panic.h"
#include "system16/memmgmt.h"
#include "system16/power.h"

struct Service {
    const char* name;
    ServiceFunc func;
    TaskHandle_t handle;
    uint16_t stack;
    bool stopRequest;
    volatile bool crashed;
};

static Service services[10];
static uint8_t count = 0;

static void serviceTaskWrapper(void* param);
static void showServiceWarning(const char* name, const char* reason);

int getServiceCount() {
    return count;
}

ServiceInfo getServiceInfo(int index) {
    if (index < 0 || index >= count) return {"<invalid>", false, 0};

    Service& s = services[index];
    size_t stackused = s.handle ? s.stack - uxTaskGetStackHighWaterMark(s.handle) : 0;
    return {s.name, s.handle != NULL && !s.stopRequest, stackused};
}

void registerService(const char* name, ServiceFunc fn, uint16_t stack) {
    if (count >= (sizeof(services) / sizeof(services[0]))) return;

    uint16_t safeStack = stack < 2048 ? 2048 : stack;
    services[count++] = {name, fn, NULL, safeStack, false, false};
}

bool serviceStopRequested(const char* name) {
    for (int i = 0; i < count; i++) {
        if (strcmp(services[i].name, name) == 0)
            return services[i].stopRequest;
    }
    return false;
}

static void serviceTaskWrapper(void* param) {
    Service* svc = (Service*)param;

    if (!svc || !svc->func) {
        showServiceWarning(svc ? svc->name : "<null>", "Invalid service");
        vTaskDelete(NULL);
        return;
    }

    while (!svc->stopRequest) {
        MEM::monitorLoop(svc->name);

        auto ev = MEM::checkTask(svc->name);
        if (ev != MEM::Event::OK) {
            svc->crashed = true;
            SYS::handleMemEvent(ev, svc->name);
            break;
        }

        auto hev = MEM::checkHeapTrend();
        if (hev != MEM::Event::OK) {
            svc->crashed = true;
            if (hev == MEM::Event::HEAP_CRITICAL) break;

            SYS::handleMemEvent(hev, svc->name);
            break;
        }

        if (svc->stopRequest) break;

        svc->func((void*)svc->name);
        powerNotifyActivity();
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    svc->handle = NULL;
    vTaskDelete(NULL);
}

void startService() {
    if (ESP.getFreeHeap() < MEM::config.minHeapThreshold) {
        panic(PANIC_MEM_CORRUPT, "Not enough heap to start services");
    }

    for (int i = 0; i < count; i++) {
        services[i].stopRequest = false;
        services[i].crashed = false;
        if (services[i].handle == NULL) {
            BaseType_t ok = xTaskCreate(
                serviceTaskWrapper,
                services[i].name,
                services[i].stack,
                (void*)&services[i],
                1,
                &services[i].handle);

            if (ok != pdPASS) {
                services[i].crashed = true;
                showServiceWarning(services[i].name, "Failed to start");
            }
        }
    }
}

void stopService(const char* name) {
    for (int i = 0; i < count; i++) {
        if (strcmp(services[i].name, name) == 0) {
            services[i].stopRequest = true;
            return;
        }
    }
}

void stopAllService() {
    for (int i = 0; i < count; i++) {
        services[i].stopRequest = true;
    }
}

static void showServiceWarning(const char* name, const char* reason) {
    display.clearBuffer();
    display.setFont(u8g2_font_6x12_tf);
    display.drawStr(0, 0, "SERVICE WARNING");
    display.drawStr(0, 16, "App crashed:");
    display.drawStr(0, 28, name ? name : "<unknown>");
    display.drawStr(0, 44, reason ? reason : "Unknown reason");
    display.sendBuffer();
}