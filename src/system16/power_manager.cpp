#include "power_manager.h"
#include "component/liveness.h"
#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_bt.h>
#include <esp_sleep.h>
#include <esp_system.h>

static inline void cfg_pm(uint32_t max_mhz, uint32_t min_mhz, bool light_sleep) {
    esp_pm_config_esp32c3_t pm = {};
    pm.max_freq_mhz = max_mhz;
    pm.min_freq_mhz = min_mhz;
    pm.light_sleep_enable = light_sleep;
    esp_pm_configure(&pm);
}

PowerManager::PowerManager() {}

void PowerManager::begin(Profile profile) {
    _profile = profile;
    _lastActivity = millis();

    applyPmConfig();
    applyWifiPolicy();
}

void PowerManager::setProfile(Profile profile) {
    if (_profile == profile) return;
    _profile = profile;
    applyPmConfig();
    applyWifiPolicy();
}

void PowerManager::applyPmConfig() {
    switch (_profile) {
        case Performance:
            cfg_pm(160, 160, false);
            break;
        case Balanced:
            cfg_pm(160, 40, true);
            break;
        case PowerSave:
            cfg_pm(80, 40, true);
            break;
    }
}

void PowerManager::applyWifiPolicy() {
    wifi_ps_type_t ps = WIFI_PS_NONE;

    switch (_profile) {
        case Performance:
            ps = WIFI_PS_NONE;
            break;
        case Balanced:
            ps = WIFI_PS_MIN_MODEM;
            break;
        case PowerSave:
            ps = WIFI_PS_MAX_MODEM;
            break;
    }

    esp_wifi_set_ps(ps);
    WiFi.setSleep(ps != WIFI_PS_NONE);
}

void PowerManager::setWifiSleep(bool enable) {
    esp_wifi_set_ps(enable ? WIFI_PS_MIN_MODEM : WIFI_PS_NONE);
    WiFi.setSleep(enable);
}

void PowerManager::setWifiEnabled(bool enable) {
    if (enable) {
        if (WiFi.getMode() == WIFI_OFF) {
            WiFi.mode(WIFI_STA);
        }
    } else {
        WiFi.mode(WIFI_OFF);
        esp_wifi_stop();
        // esp_wifi_deinit(); // Deinit might be too aggressive if we want to restart quickly
    }
}

void PowerManager::notifyActivity() {
    _lastActivity = millis();
    if (_isDimmed) {
        _isDimmed = false;
        if (_displayDim) _displayDim(false);
    }
}

void PowerManager::loop() {
    uint32_t now = millis();
    uint32_t idle = now - _lastActivity;

    if (_autoSleepMs == 0 || _perfLocks > 0) return;

    // Smart Dimming: Lower power after 15s of inactivity
    if (idle > 15000 && !_isDimmed) {
        _isDimmed = true;
        if (_displayDim) _displayDim(true);
    }

    // Auto WiFi Shutdown: Turn off WiFi if idle and not connected for 3 mins
    static uint32_t lastWifiCheck = 0;
    if (now - lastWifiCheck > 10000) {
        lastWifiCheck = now;
        if (WiFi.getMode() != WIFI_OFF && WiFi.status() != WL_CONNECTED && idle > 180000) {
            setWifiEnabled(false);
            Liveness::Notifications::show("WiFi: Power Saved");
            Serial.println("[PM] Auto WiFi shutdown due to inactivity");
        }
    }

    if (idle >= _autoSleepMs) {
        lightSleep();
        _lastActivity = millis();
    }
}

void PowerManager::lightSleep(uint64_t wake_gpio_mask, bool wake_low) {
    if (_displayPower) _displayPower(false);
    if (_preSleep) _preSleep();

    if (wake_gpio_mask) {
        gpio_deep_sleep_hold_dis();

        for (int gpio = 0; gpio < 32; ++gpio) {
            if (wake_gpio_mask & (1ULL << gpio)) {
                gpio_wakeup_enable((gpio_num_t)gpio, wake_low ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
            }
        }
        esp_sleep_enable_gpio_wakeup();
    } else {
        esp_sleep_enable_timer_wakeup(1 * 1000ULL);
    }

    WiFi.setSleep(true);

    esp_light_sleep_start();

    WiFi.setSleep(_profile != Performance);

    if (_postWake) _postWake();
    if (_displayPower) _displayPower(true);
}

void PowerManager::deepSleep(uint64_t wake_gpio_mask, bool wake_low) {
    if (_displayPower) _displayPower(false);
    if (_preSleep) _preSleep();

    WiFi.mode(WIFI_OFF);
    esp_wifi_stop();
    esp_wifi_deinit();
    btStop();

    if (wake_gpio_mask) {
        if (wake_low) {
            esp_deep_sleep_enable_gpio_wakeup(wake_gpio_mask, ESP_GPIO_WAKEUP_GPIO_LOW);
        } else {
            esp_deep_sleep_enable_gpio_wakeup(wake_gpio_mask, ESP_GPIO_WAKEUP_GPIO_HIGH);
        }
    }

    esp_deep_sleep_start();
}

void PowerManager::acquireHighPerf() {
    if (++_perfLocks == 1) {
        cfg_pm(160, 160, false);
        esp_wifi_set_ps(WIFI_PS_NONE);
    }
}

void PowerManager::releaseHighPerf() {
    if (_perfLocks == 0) return;
    if (--_perfLocks == 0) {
        applyPmConfig();
        applyWifiPolicy();
    }
}
