#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

/**
 * @brief Structure to store crash diagnostic data
 */
struct CrashLog {
    char appName[32];      // Name of the app that crashed
    uint32_t freeHeap;     // Free heap at crash time
    uint8_t resetReason;   // esp_reset_reason_t
    uint32_t errorCode;    // Custom error code or PC
    uint32_t timestamp;    // Uptime in ms
    bool valid;            // Flag to check if log is valid
};

/**
 * @brief Engine states for Safe-Mode
 */
enum RecoveryState {
    RECOVERY_NORMAL,
    RECOVERY_SAFE_MODE,
    RECOVERY_PANIC
};

class CrashEngine {
public:
    static const char* LOG_PATH;
    static const char* CONFIG_PATH;

    /**
     * @brief Initialize the crash engine, check for existing logs
     */
    static void begin();

    /**
     * @brief Intercept and log a system failure
     * 
     * @param appName Name of the faulted application
     * @param errorCode Custom error code or PC
     */
    static void captureCrash(const char* appName, uint32_t errorCode);

    /**
     * @brief Check if we should boot into Safe-Mode
     */
    static bool shouldEnterSafeMode();

    /**
     * @brief Load the last crash log from LittleFS
     */
    static bool loadLog(CrashLog& log);

    /**
     * @brief Clear the crash log and reboot
     */
    static void clearAndReboot();

    /**
     * @brief Configuration for core dumps
     */
    static void setCoreDumpEnabled(bool enabled);
    static bool isCoreDumpEnabled();

    /**
     * @brief Fault Simulations
     */
    static void simulateMemoryLeak();
    static void simulateNullPointer();
    static void simulateWatchdogTimeout();
    static void simulateSelfDestruct();  // Self-destruct test

    static void saveLog(const CrashLog& log);

private:
    static bool _coreDumpEnabled;
};
