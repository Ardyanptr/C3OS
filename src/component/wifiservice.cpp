#include "wifiservice.h"
#include "liveness.h"
#include "service.h"

WiFiStats g_wifiStats = {0.0f, 0.0f, 0, 0, false};

void WiFiMonitorService(void* param) {
    bool lastConnected = false;

    // Wait for WiFi to be initialized
    while (WiFi.status() == WL_NO_SHIELD) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    while (true) {
        bool connected = (WiFi.status() == WL_CONNECTED);
        
        if (connected && !lastConnected) {
            Liveness::Notifications::show("WiFi Connected: " + WiFi.SSID());
        } else if (!connected && lastConnected) {
            Liveness::Notifications::show("WiFi Disconnected");
        }
        lastConnected = connected;
        g_wifiStats.connected = connected;

        // Note: esp_netif_get_stats is available in IDF 5.0+, 
        // but not in the current IDF 4.4.x used by this framework.
        // For now, we set speeds to 0.
        g_wifiStats.downloadSpeed = 0;
        g_wifiStats.uploadSpeed = 0;
        g_wifiStats.totalRX = 0;
        g_wifiStats.totalTX = 0;

        updateServiceHeartbeat("WiFiMonitor");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
