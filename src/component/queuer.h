#pragma once
#include <Arduino.h>

#include <functional>
#include <queue>

#define MAX_WORKERS 5
#define WORKER_STACK 1024

using Job = std::function<void()>;

std::queue<Job> jobQueue;
SemaphoreHandle_t queueMutex;

inline void workerTask(void* arg) {
    for (;;) {
        Job job = nullptr;

        xSemaphoreTake(queueMutex, portMAX_DELAY);
        if (!jobQueue.empty()) {
            job = jobQueue.front();
            jobQueue.pop();
        }
        xSemaphoreGive(queueMutex);

        if (job) job();
        vTaskDelay(1);
    }
}

inline void addBackgroundJob(Job job) {
    xSemaphoreTake(queueMutex, portMAX_DELAY);
    jobQueue.push(job);
    xSemaphoreGive(queueMutex);
}

inline void initBackgroundManager() {
    queueMutex = xSemaphoreCreateMutex();
    for (int i = 0; i < MAX_WORKERS; i++) {
        xTaskCreate(workerTask, "Worker", WORKER_STACK, nullptr, 1, nullptr);
    }

    xTaskCreate([](void* args) {
        for (;;) {
            if (Serial.available()) {
                String cmd = Serial.readStringUntil('\n');
                cmd.trim();

                if (cmd == "PING") {
                    Serial.println("PONG");
                } else if (cmd == "REBOOT") {
                    Serial.println("Rebooting via software...");
                    delay(500);
                    esp_restart();
                }
            }
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    },
                "SerialRecovery", 2048, nullptr, 1, nullptr);
}