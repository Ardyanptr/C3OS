#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <esp_system.h>

/**
 * @brief Security Kernel for C3OS
 * Provides corruption detection, self-healing capabilities, and critical data backup
 */

// Magic signatures for integrity verification
#define SECURITY_MAGIC_VALID    0x53454352  // "SECR"
#define SECURITY_MAGIC_CORRUPT  0xDEAD      // Corrupted marker
#define SECURITY_BACKUP_MAGIC   0x42414350  // "BACP"

// Maximum backup entries
#define MAX_BACKUP_ENTRIES      10
#define MAX_PATH_LEN            32

// Security status codes
enum SecurityStatus {
    SEC_STATUS_OK,              // System is healthy
    SEC_STATUS_WARNING,         // Minor issues detected
    SEC_STATUS_CRITICAL,        // Critical corruption detected
    SEC_STATUS_RECOVERED,       // Successfully recovered
    SEC_STATUS_UNRECOVERABLE    // Cannot recover
};

// Corruption types
enum CorruptionType {
    CORRUPT_NONE,
    CORRUPT_FILESYSTEM,
    CORRUPT_SETTINGS,
    CORRUPT_BOOTLOADER,
    CORRUPT_RTC_MEMORY,
    CORRUPT_SYSTEM_CONFIG
};

/**
 * @brief Structure for storing backup data
 */
struct BackupEntry {
    char path[MAX_PATH_LEN];    // File path
    uint32_t size;              // File size
    uint32_t timestamp;         // Backup timestamp
    uint32_t checksum;          // CRC32 checksum
    bool valid;                 // Entry validity flag
};

/**
 * @brief Structure for security diagnostic report
 */
struct SecurityReport {
    SecurityStatus status;
    CorruptionType corruptionType;
    uint32_t integrityScore;    // 0-100% system integrity
    uint32_t freeHeap;
    uint32_t freeFlash;
    bool fsHealthy;
    bool settingsValid;
    bool backupAvailable;
    uint32_t lastCheckTime;
    char errorMsg[64];
};

/**
 * @brief Security Kernel class
 * Handles system integrity monitoring, corruption recovery, and data backup
 */
class SecurityKernel {
public:
    // Singleton access
    static SecurityKernel* getInstance();
    
    // Initialization
    static void begin();
    
    // Integrity checking
    static SecurityStatus checkIntegrity();
    static SecurityStatus checkIntegrity(SecurityReport& report);
    static bool checkFilesystem();
    static bool checkSettings();
    static bool checkRTC();
    
    // Recovery functions
    static bool attemptRecovery();
    static bool attemptRecovery(CorruptionType type);
    static bool recoverFilesystem();
    static bool recoverSettings();
    static bool recoverFromBackup();
    
    // Backup management
    static bool backupCriticalSettings();
    static bool restoreCriticalSettings();
    static bool backupFile(const char* path);
    static bool restoreFile(const char* path);
    static bool listBackups(BackupEntry* entries, int& count);
    static bool clearOldBackups();
    
    // Self-destruct simulation (for testing)
    static void triggerSelfDestruct();
    static void simulateCorruption(CorruptionType type);
    
    // System protection
    static void enableWatchdog();
    static void disableWatchdog();
    static void safeRestart();
    
    // Diagnostic
    static void generateReport(SecurityReport& report);
    static void printReport(const SecurityReport& report);
    
    // Checksum utilities
    static uint32_t calculateChecksum(const uint8_t* data, size_t len);
    static bool verifyFileChecksum(const char* path, uint32_t expected);
    
private:
    static SecurityKernel* _instance;
    
    static const char* BACKUP_DIR;
    static const char* SECURITY_LOG;
    static const char* INTEGRITY_FILE;
    
    static SecurityStatus _currentStatus;
    static CorruptionType _lastCorruption;
    static uint32_t _lastCheckTime;
    static bool _initialized;
    
    // Internal helpers
    static bool mountFS();
    static bool writeIntegrityRecord();
    static bool readIntegrityRecord();
    static void logEvent(const char* event);
    static CorruptionType detectCorruptionType();
};

// Global convenience functions
inline SecurityKernel* Security() { return SecurityKernel::getInstance(); }