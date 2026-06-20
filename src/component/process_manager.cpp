#include "process_manager.h"
#include <esp_task_wdt.h>

static void appTaskWrapper(void* param) {
    Process* proc = (Process*)param;
    AppDesc& app = appTable[proc->appIndex];
    
    Serial.printf("[PROC] Starting %s (ID: %d)\n", app.name, proc->id);
    
    // Ensure display is cleared before app starts
    display.clearBuffer();
    display.sendBuffer();
    
    app.entry();
    
    Serial.printf("[PROC] Finished %s (ID: %d)\n", app.name, proc->id);
    proc->state = PROC_FINISHED;
    vTaskDelete(NULL);
}

int ProcessManager::launchApp(int appIndex) {
    if (appIndex < 0 || appIndex >= APP_COUNT) return -1;
    
    // Check if already running
    for (auto& proc : processes) {
        if (proc.appIndex == appIndex && proc.state != PROC_FINISHED && proc.state != PROC_KILLED) {
            setForeground(proc.id);
            return proc.id;
        }
    }

    // Low Memory Killer (LMK)
    AppDesc& targetApp = appTable[appIndex];
    while (ESP.getFreeHeap() < targetApp.minHeap && !processes.empty()) {
        int toKillId = -1;
        String toKillName = "";
        for (auto& proc : processes) {
            if (proc.state == PROC_BACKGROUND) {
                toKillId = proc.id;
                toKillName = proc.name;
                break;
            }
        }
        if (toKillId == -1) break; // No background processes left
        
        Serial.printf("[LMK] Killing background process %s (ID: %d) to free memory\n", 
                      toKillName.c_str(), toKillId);
        killProcess(toKillId);
    }

    Process proc;
    proc.id = nextId++;
    proc.appIndex = appIndex;
    proc.name = appTable[appIndex].name;
    proc.state = PROC_RUNNING;
    proc.startTime = millis();
    proc.lastHeartbeat = millis();
    
    processes.push_back(proc);
    Process* procPtr = &processes.back();

    BaseType_t res = xTaskCreate(
        appTaskWrapper,
        procPtr->name,
        8192, // Increased stack for apps
        (void*)procPtr,
        1,
        &procPtr->handle
    );

    if (res != pdPASS) {
        processes.pop_back();
        return -1;
    }

    foregroundId = procPtr->id;
    return procPtr->id;
}

void ProcessManager::killProcess(int id) {
    for (auto it = processes.begin(); it != processes.end(); ++it) {
        if (it->id == id) {
            if (it->handle) {
                vTaskDelete(it->handle);
            }
            it->state = PROC_KILLED;
            if (foregroundId == id) foregroundId = -1;
            processes.erase(it);
            return;
        }
    }
}

void ProcessManager::setForeground(int id) {
    for (auto& proc : processes) {
        if (proc.id == id) {
            proc.state = PROC_RUNNING;
            foregroundId = id;
            if (proc.handle != NULL) {
                Serial.printf("[PROC] Resuming task %s (ID: %d)\n", proc.name, proc.id);
                vTaskResume(proc.handle);
            }
        } else if (proc.state == PROC_RUNNING) {
            proc.state = PROC_BACKGROUND;
            // The task will suspend itself on its next heartbeat
        }
    }
}

void ProcessManager::update() {
    // Cleanup finished processes
    for (auto it = processes.begin(); it != processes.end(); ) {
        if (it->state == PROC_FINISHED || it->state == PROC_KILLED) {
            if (foregroundId == it->id) foregroundId = -1;
            it = processes.erase(it);
        } else {
            ++it;
        }
    }
}

static bool isCurrentTaskForeground() {
    int fgId = ProcessManager::instance().getForegroundId();
    if (fgId == -1) return true; // System Home

    TaskHandle_t current = xTaskGetCurrentTaskHandle();
    auto& procs = ProcessManager::instance().getProcesses();
    for (auto& p : procs) {
        if (p.handle == current) {
            return p.id == fgId;
        }
    }
    return true; // System services
}

void C3OSDisplay::sendBuffer() {
    if (isCurrentTaskForeground()) {
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            U8G2_SSD1315_128X64_NONAME_F_HW_I2C::sendBuffer();
            xSemaphoreGive(i2cMutex);
        }
    }
}

void C3OSDisplay::clearBuffer() {
    if (isCurrentTaskForeground()) {
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            U8G2_SSD1315_128X64_NONAME_F_HW_I2C::clearBuffer();
            xSemaphoreGive(i2cMutex);
        }
    }
}
