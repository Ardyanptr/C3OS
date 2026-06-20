#include "service.h"

#include <string.h>

#include "panic.h"

struct Service {
    const char* name;
    ServiceFunc func;
    TaskHandle_t handle;
    uint16_t stack;
    ServicePriority priority;
    bool stopRequest;
    uint32_t lastHeartbeat;
    SemaphoreHandle_t lock;  // Untuk thread safety
};

static Service services[12];
static uint8_t count = 0;

void registerService(const char* name, ServiceFunc fn, uint16_t stack, ServicePriority prio) {
    if (count >= 12) return;

    services[count].name = name;
    services[count].func = fn;
    services[count].handle = NULL;
    services[count].stack = (stack < 2048) ? 2048 : stack;
    services[count].priority = prio;
    services[count].stopRequest = false;
    services[count].lastHeartbeat = millis();
    services[count].lock = xSemaphoreCreateMutex();  // Init lock
    count++;
}

// Service Wrapper dengan proteksi lebih ketat
static void serviceTaskWrapper(void* param) {
    Service* svc = (Service*)param;
    const char* svcName = svc->name;

    for (;;) {
        if (svc->stopRequest) break;

        // Heartbeat check: Jika tidak update dalam 30 detik, anggap hang
        if (millis() - svc->lastHeartbeat > 30000) {
            if (svc->priority == ServicePriority::PRIO_CRITICAL) {
                panic(PANIC_IPC_DROP, "Critical Service Hang");
            }
        }

        // Jalankan fungsi service
        svc->func((void*)svcName);

        // Beri waktu bagi scheduler agar tidak starving
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    xSemaphoreTake(svc->lock, portMAX_DELAY);
    svc->handle = NULL;
    xSemaphoreGive(svc->lock);
    vTaskDelete(NULL);
}

void startService() {
    for (int i = 0; i < count; i++) {
        // Jangan start jika RAM kritis
        if (ESP.getFreeHeap() < 20000) {
            Serial.printf("[!] Critically low memory, skipping %s\n", services[i].name);
            continue;
        }

        xSemaphoreTake(services[i].lock, portMAX_DELAY);
        if (services[i].handle == NULL) {
            services[i].stopRequest = false;
            services[i].lastHeartbeat = millis();

            BaseType_t ok = xTaskCreate(
                serviceTaskWrapper,
                services[i].name,
                services[i].stack,
                (void*)&services[i],
                (uint8_t)services[i].priority,  // Menggunakan priority yang didaftarkan
                &services[i].handle);

            if (ok != pdPASS) {
                Serial.printf("[ERR] Failed to start: %s\n", services[i].name);
            } else {
                // STAGGERED START: Jeda 100ms antar service
                // Ini mencegah lonjakan CPU/RAM yang bikin crash
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
        xSemaphoreGive(services[i].lock);
    }
}

void updateServiceHeartbeat(const char* name) {
    for (int i = 0; i < count; i++) {
        if (strcmp(services[i].name, name) == 0) {
            services[i].lastHeartbeat = millis();
            return;
        }
    }
}

int getServiceCount() {
    return count;
}

ServiceInfo getServiceInfo(int index) {
    if (index < 0 || index >= count) {
        return {"<invalid>", false, 0, 0};
    }

    Service& s = services[index];
    size_t stackUsed = 0;

    // Hitung sisa stack jika task sedang berjalan
    if (s.handle != NULL) {
        stackUsed = s.stack - uxTaskGetStackHighWaterMark(s.handle);
    }

    return {s.name, (s.handle != NULL), stackUsed, s.lastHeartbeat};
}

bool serviceStopRequested(const char* name) {
    for (int i = 0; i < count; i++) {
        if (strcmp(services[i].name, name) == 0) {
            return services[i].stopRequest;
        }
    }
    return false;
}

void stopAllService() {
    for (int i = 0; i < count; i++) {
        services[i].stopRequest = true;
    }
    Serial.println("[SYSTEM] All services stop requested");
}

bool stopService(const char* name) {
    for (int i = 0; i < count; i++) {
        if (strcmp(services[i].name, name) == 0) {
            xSemaphoreTake(services[i].lock, portMAX_DELAY);

            if (services[i].handle != NULL) {
                services[i].stopRequest = true;
            }

            xSemaphoreGive(services[i].lock);
            return true;
        }
    }
    return false;
}

bool startServiceByName(const char* name) {
    for (int i = 0; i < count; i++) {
        if (strcmp(services[i].name, name) == 0) {
            xSemaphoreTake(services[i].lock, portMAX_DELAY);

            if (services[i].handle == NULL) {
                services[i].stopRequest = false;
                services[i].lastHeartbeat = millis();

                BaseType_t ok = xTaskCreate(
                    serviceTaskWrapper,
                    services[i].name,
                    services[i].stack,
                    (void*)&services[i],
                    (uint8_t)services[i].priority,
                    &services[i].handle);

                xSemaphoreGive(services[i].lock);
                return ok == pdPASS;
            }

            xSemaphoreGive(services[i].lock);
        }
    }

    return false;
}