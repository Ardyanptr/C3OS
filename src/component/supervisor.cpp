#include <Arduino.h>
#include "supervisor.h"

static TaskHandle_t appTask = nullptr;
static volatile bool appFinished = false;

static void appTaskWrapper(void* param) {
    AppEntry entry = (AppEntry)param;
    entry();
    appFinished = true;
    vTaskDelete(nullptr);
}

bool supervisedRun(AppEntry entry, const char* name, uint32_t timeoutMs) {
    appFinished = false;
    
    xTaskCreate(appTaskWrapper, name, 4096, (void*)entry, 1, &appTask);

    uint32_t start = millis();
    while(millis() - start < timeoutMs) {
        if (appFinished) return true;
        vTaskDelay(10);
    }

    vTaskDelete(appTask);
    appTask = nullptr;
    return false;
}