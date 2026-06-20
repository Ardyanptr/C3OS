#include "CrashEngine.h"
#include <ArduinoJson.h>

const char* CrashEngine::LOG_PATH = "/cfg/crash_metrics.json";
const char* CrashEngine::CONFIG_PATH = "/cfg/crash_config.json";
bool CrashEngine::_coreDumpEnabled = true;

// Use RTC memory to survive across reboots (if not power-cycled)
RTC_NOINIT_ATTR CrashLog rtcCrashLog;
RTC_NOINIT_ATTR uint32_t rtcCrashMagic;

#define CRASH_MAGIC 0x5AFE106 // "SAFE LOG"

void CrashEngine::begin() {
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
        return;
    }

    // Load config
    if (LittleFS.exists(CONFIG_PATH)) {
        File f = LittleFS.open(CONFIG_PATH, "r");
        JsonDocument doc;
        deserializeJson(doc, f);
        _coreDumpEnabled = doc["coreDump"] | true;
        f.close();
    }

    // Check if we have a pending crash in RTC memory
    if (rtcCrashMagic == CRASH_MAGIC) {
        if (_coreDumpEnabled) {
            saveLog(rtcCrashLog);
        }
        rtcCrashMagic = 0; // Clear magic
    }
}

void CrashEngine::captureCrash(const char* appName, uint32_t errorCode) {
    rtcCrashLog.valid = true;
    strncpy(rtcCrashLog.appName, appName, 31);
    rtcCrashLog.appName[31] = '\0';
    rtcCrashLog.freeHeap = ESP.getFreeHeap();
    rtcCrashLog.resetReason = (uint8_t)esp_reset_reason();
    rtcCrashLog.errorCode = errorCode;
    rtcCrashLog.timestamp = millis();
    
    rtcCrashMagic = CRASH_MAGIC;

    // Try to write to FS immediately if we are not in a hard panic
    // (Software simulated crashes)
    if (_coreDumpEnabled) {
        saveLog(rtcCrashLog);
        rtcCrashMagic = 0; // Already saved
    }

    Serial.printf("CRASH CAPTURED: %s, Code: 0x%X\n", appName, errorCode);
    
    delay(100);
    esp_restart();
}

bool CrashEngine::shouldEnterSafeMode() {
    return LittleFS.exists(LOG_PATH);
}

bool CrashEngine::loadLog(CrashLog& log) {
    if (!LittleFS.exists(LOG_PATH)) return false;

    File f = LittleFS.open(LOG_PATH, "r");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, f);
    f.close();

    if (error) return false;

    strncpy(log.appName, doc["app"] | "Unknown", 31);
    log.freeHeap = doc["heap"] | 0;
    log.resetReason = doc["reason"] | 0;
    log.errorCode = doc["code"] | 0;
    log.timestamp = doc["time"] | 0;
    log.valid = true;

    return true;
}

void CrashEngine::clearAndReboot() {
    LittleFS.remove(LOG_PATH);
    rtcCrashMagic = 0;
    esp_restart();
}

void CrashEngine::setCoreDumpEnabled(bool enabled) {
    _coreDumpEnabled = enabled;
    File f = LittleFS.open(CONFIG_PATH, "w");
    JsonDocument doc;
    doc["coreDump"] = enabled;
    serializeJson(doc, f);
    f.close();
}

bool CrashEngine::isCoreDumpEnabled() {
    return _coreDumpEnabled;
}

void CrashEngine::saveLog(const CrashLog& log) {
    File f = LittleFS.open(LOG_PATH, "w");
    JsonDocument doc;
    doc["app"] = log.appName;
    doc["heap"] = log.freeHeap;
    doc["reason"] = log.resetReason;
    doc["code"] = log.errorCode;
    doc["time"] = log.timestamp;
    serializeJson(doc, f);
    f.close();
}

// Simulations
void CrashEngine::simulateMemoryLeak() {
    Serial.println("Simulating Memory Leak...");
    while (true) {
        void* p = malloc(1024);
        if (p == NULL) {
            captureCrash("SimLeak", 0x1EAC);
            break;
        }
        memset(p, 0xAA, 1024);
    }
}

void CrashEngine::simulateNullPointer() {
    Serial.println("Simulating Null Pointer...");
    int* p = NULL;
    *p = 123; // This will trigger a panic on most ESP32-C3 configurations
    // If it doesn't immediately panic, we call captureCrash manually for the simulation
    captureCrash("SimNull", 0x0000);
}

void CrashEngine::simulateWatchdogTimeout() {
    Serial.println("Simulating Watchdog Timeout...");
    // Disable WDT reset in this loop to force hardware WDT if enabled
    while (true) {
        // Just spin
    }
}

void CrashEngine::simulateSelfDestruct() {
    Serial.println("!!! SIMULATING SELF-DESTRUCT !!!");
    
    // Capture crash with self-destruct marker
    captureCrash("SelfDestruct", 0xDE5701D);
}
