#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "driver/rtc_io.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "esp_wifi.h"

class PowerManager {
   public:
    enum Profile {
        Performance,
        Balanced,
        PowerSave
    };

    typedef void (*Callback)();
    typedef void (*DisplayPowerCallback)(bool on);
    typedef void (*DisplayDimCallback)(bool dim);

    PowerManager();

    void begin(Profile profile = Balanced);

    void setProfile(Profile profile);
    Profile getProfile() const { return _profile; }

    void setPreSleepCallback(Callback cb) { _preSleep = cb; }
    void setPostWakeCallback(Callback cb) { _postWake = cb; }
    void setDisplayPowerCallback(DisplayPowerCallback cb) { _displayPower = cb; }
    void setDisplayDimCallback(DisplayDimCallback cb) { _displayDim = cb; }

    void setWifiSleep(bool enable);
    void setWifiEnabled(bool enable);

    void notifyActivity();
    void setAutoSleepTimeout(uint32_t ms) { _autoSleepMs = ms; }
    uint32_t getAutoSleepTimeout() const { return _autoSleepMs; }

    void loop();

    void lightSleep(uint64_t wake_gpio_mask = 0ULL, bool wake_low = true);
    void deepSleep(uint64_t wake_gpio_mask = 0ULL, bool wake_low = true);

    void acquireHighPerf();
    void releaseHighPerf();

   private:
    void applyPmConfig();
    void applyWifiPolicy();

    Profile _profile = Balanced;

    uint32_t _autoSleepMs = 120000;
    uint32_t _lastActivity = 0;

    uint8_t _perfLocks = 0;

    Callback _preSleep = nullptr;
    Callback _postWake = nullptr;
    DisplayPowerCallback _displayPower = nullptr;
    DisplayDimCallback _displayDim = nullptr;
    bool _isDimmed = false;
};
