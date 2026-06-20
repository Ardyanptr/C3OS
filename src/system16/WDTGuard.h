#pragma once
#include <Arduino.h>
#include <esp_task_wdt.h>

class WDTGuard {
   public:
    static void begin() {
        // Create high-priority task that just feeds WDT
        xTaskCreate([](void*) {
            while (true) {
                esp_task_wdt_reset();
                delay(500);  // Feed every 500ms
            }
        },
                    "wdt_feeder", 1024, NULL, 1, NULL);  // Low priority, won't disturb game
    }
};