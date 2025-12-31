#include <Arduino.h>
#include <WiFi.h>

#include "app.h"
#include "esp_wifi.h"

void applyCapabilityBoundary(uint32_t caps) {
    if (!(caps & CAP_WIFI)) {
        WiFi.mode(WIFI_OFF);
        esp_wifi_stop();
    }

    if (!(caps & CAP_BT)) {
        btStop();
    }
}
