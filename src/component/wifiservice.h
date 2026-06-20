#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "esp_netif.h"

struct WiFiStats {
    float downloadSpeed; // KB/s
    float uploadSpeed;   // KB/s
    uint32_t totalRX;    // Bytes
    uint32_t totalTX;    // Bytes
    bool connected;
};

extern WiFiStats g_wifiStats;

void WiFiMonitorService(void* param);
