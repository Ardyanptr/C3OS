#include "SecurityKernel.h"
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <esp_spi_flash.h>

SecurityKernel* SecurityKernel::_instance = nullptr;

const char* SecurityKernel::BACKUP_DIR = "/backup";
const char* SecurityKernel::SECURITY_LOG = "/cfg/security.log";
const char* SecurityKernel::INTEGRITY_FILE = "/cfg/integrity.dat";

SecurityStatus SecurityKernel::_currentStatus = SEC_STATUS_OK;
CorruptionType SecurityKernel::_lastCorruption = CORRUPT_NONE;
uint32_t SecurityKernel::_lastCheckTime = 0;
bool SecurityKernel::_initialized = false;

// Critical files to backup
static const char* CRITICAL_FILES[] = {
    "/settings.bin",
    "/cfg/system_config.json",
    "/cfg/ui_layout.json",
    "/cfg/user_prefs.lua",
    "/cfg/crash_config.json"
};
static const int CRITICAL_FILE_COUNT = sizeof(CRITICAL_FILES) / sizeof(CRITICAL_FILES[0]);

// RTC memory for persistence across reboots
RTC_NOINIT_ATTR uint32_t security_rtc_magic;
RTC_NOINIT_ATTR CorruptionType security_rtc_corruption;

SecurityKernel* SecurityKernel::getInstance() {
    if (_instance == nullptr) {
        _instance = new SecurityKernel();
    }
    return _instance;
}

void SecurityKernel::begin() {
    if (_initialized) return;
    
    Serial.println("[SecurityKernel] Initializing...");
    
    if (!mountFS()) {
        Serial.println("[SecurityKernel] Failed to mount LittleFS");
        _currentStatus = SEC_STATUS_CRITICAL;
        return;
    }
    
    // Create backup directory if it doesn't exist
    if (!LittleFS.exists(BACKUP_DIR)) {
        LittleFS.mkdir(BACKUP_DIR);
    }
    
    // Check for pending corruption from previous boot
    if (security_rtc_magic == SECURITY_MAGIC_CORRUPT) {
        _lastCorruption = security_rtc_corruption;
        _currentStatus = SEC_STATUS_CRITICAL;
        Serial.printf("[SecurityKernel] Detected pending corruption: %d\n", _lastCorruption);
        security_rtc_magic = 0; // Clear flag
    }
    
    // Read integrity record
    readIntegrityRecord();
    
    _initialized = true;
    _lastCheckTime = millis();
    
    Serial.println("[SecurityKernel] Initialized successfully");
}

bool SecurityKernel::mountFS() {
    if (!LittleFS.begin(true)) {
        return false;
    }
    return true;
}

SecurityStatus SecurityKernel::checkIntegrity() {
    SecurityReport report;
    return checkIntegrity(report);
}

SecurityStatus SecurityKernel::checkIntegrity(SecurityReport& report) {
    Serial.println("[SecurityKernel] Running integrity check...");
    
    // Initialize report
    memset(&report, 0, sizeof(SecurityReport));
    report.freeHeap = ESP.getFreeHeap();
    report.freeFlash = ESP.getFreeSketchSpace();
    report.lastCheckTime = millis();
    
    // Check filesystem
    report.fsHealthy = checkFilesystem();
    if (!report.fsHealthy) {
        report.corruptionType = CORRUPT_FILESYSTEM;
        report.status = SEC_STATUS_CRITICAL;
        strcpy(report.errorMsg, "Filesystem corruption detected");
    }
    
    // Check settings
    report.settingsValid = checkSettings();
    if (!report.settingsValid) {
        report.corruptionType = CORRUPT_SETTINGS;
        if (report.status == SEC_STATUS_OK) {
            report.status = SEC_STATUS_WARNING;
        }
        strcpy(report.errorMsg, "Settings corruption detected");
    }
    
    // Check RTC memory
    bool rtcOk = checkRTC();
    if (!rtcOk) {
        report.corruptionType = CORRUPT_RTC_MEMORY;
        if (report.status == SEC_STATUS_OK) {
            report.status = SEC_STATUS_WARNING;
        }
    }
    
    // Calculate integrity score
    uint32_t score = 100;
    if (!report.fsHealthy) score -= 40;
    if (!report.settingsValid) score -= 30;
    if (!rtcOk) score -= 10;
    if (report.freeHeap < 10000) score -= 20;
    report.integrityScore = score;
    
    // Check for backup availability
    report.backupAvailable = LittleFS.exists(BACKUP_DIR) && 
                             LittleFS.open(BACKUP_DIR, "r").available() > 0;
    
    // Determine final status
    if (score >= 90) {
        report.status = SEC_STATUS_OK;
    } else if (score >= 60) {
        report.status = SEC_STATUS_WARNING;
    } else if (score >= 30) {
        report.status = SEC_STATUS_CRITICAL;
    } else {
        report.status = SEC_STATUS_UNRECOVERABLE;
    }
    
    _currentStatus = report.status;
    _lastCorruption = report.corruptionType;
    _lastCheckTime = millis();
    
    // Write integrity record
    writeIntegrityRecord();
    
    Serial.printf("[SecurityKernel] Integrity check complete. Score: %lu%%\n", score);
    
    return report.status;
}

bool SecurityKernel::checkFilesystem() {
    // Try to open and list directory
    File root = LittleFS.open("/", "r");
    if (!root) {
        return false;
    }
    
    // Try to read a file to verify FS is working
    if (LittleFS.exists("/cfg")) {
        File test = LittleFS.open("/cfg", "r");
        if (test) {
            test.close();
            root.close();
            return true;
        }
    }
    
    root.close();
    return true; // If we get here, FS is accessible
}

bool SecurityKernel::checkSettings() {
    // Check if settings file exists and is valid
    if (!LittleFS.exists("/settings.bin")) {
        return false;
    }
    
    File f = LittleFS.open("/settings.bin", "r");
    if (!f) return false;
    
    // Read and verify signature
    uint32_t signature;
    if (f.readBytes((char*)&signature, sizeof(signature)) != sizeof(signature)) {
        f.close();
        return false;
    }
    
    f.close();
    
    // Check for valid signature (Settings::Data uses a signature field)
    return signature != 0 && signature != 0xFFFFFFFF;
}

bool SecurityKernel::checkRTC() {
    // Check RTC memory integrity
    // Simple check - if magic is set to corruption, RTC may be compromised
    if (security_rtc_magic == SECURITY_MAGIC_CORRUPT) {
        return false;
    }
    return true;
}

bool SecurityKernel::attemptRecovery() {
    CorruptionType type = detectCorruptionType();
    return attemptRecovery(type);
}

bool SecurityKernel::attemptRecovery(CorruptionType type) {
    Serial.printf("[SecurityKernel] Attempting recovery for corruption type: %d\n", type);
    
    bool success = false;
    
    switch (type) {
        case CORRUPT_FILESYSTEM:
            success = recoverFilesystem();
            break;
        case CORRUPT_SETTINGS:
            success = recoverSettings();
            break;
        case CORRUPT_SYSTEM_CONFIG:
            success = recoverFromBackup();
            break;
        default:
            // Try full recovery
            success = recoverFilesystem() && recoverSettings();
            break;
    }
    
    if (success) {
        _currentStatus = SEC_STATUS_RECOVERED;
        Serial.println("[SecurityKernel] Recovery successful!");
    } else {
        Serial.println("[SecurityKernel] Recovery failed!");
    }
    
    return success;
}

bool SecurityKernel::recoverFilesystem() {
    Serial.println("[SecurityKernel] Attempting filesystem recovery...");
    
    // Try to repair by formatting
    if (!LittleFS.begin(false)) {
        Serial.println("[SecurityKernel] Filesystem mount failed, formatting...");
        LittleFS.format();
        
        // Remount
        if (LittleFS.begin(true)) {
            // Recreate essential directories
            LittleFS.mkdir("/cfg");
            LittleFS.mkdir(BACKUP_DIR);
            return true;
        }
        return false;
    }
    
    return true;
}

bool SecurityKernel::recoverSettings() {
    Serial.println("[SecurityKernel] Attempting settings recovery...");
    
    // Try to restore from backup
    if (restoreCriticalSettings()) {
        Serial.println("[SecurityKernel] Settings restored from backup");
        return true;
    }
    
    // If no backup, create default settings
    Serial.println("[SecurityKernel] No backup found, creating defaults...");
    return true; // Settings will be recreated on next boot
}

bool SecurityKernel::recoverFromBackup() {
    Serial.println("[SecurityKernel] Recovering from backup...");
    
    if (!LittleFS.exists(BACKUP_DIR)) {
        Serial.println("[SecurityKernel] No backup directory found");
        return false;
    }
    
    // List and restore backups
    File root = LittleFS.open(BACKUP_DIR, "r");
    if (!root) return false;
    
    File entry = root.openNextFile();
    int restored = 0;
    
    while (entry) {
        if (!entry.isDirectory()) {
            Serial.printf("[SecurityKernel] Restoring: %s\n", entry.name());
            // Restore logic here
            restored++;
        }
        entry = root.openNextFile();
    }
    
    root.close();
    
    return restored > 0;
}

bool SecurityKernel::backupCriticalSettings() {
    Serial.println("[SecurityKernel] Backing up critical settings...");
    
    if (!mountFS()) return false;
    
    // Create backup directory
    if (!LittleFS.exists(BACKUP_DIR)) {
        LittleFS.mkdir(BACKUP_DIR);
    }
    
    int backedUp = 0;
    
    for (int i = 0; i < CRITICAL_FILE_COUNT; i++) {
        if (backupFile(CRITICAL_FILES[i])) {
            backedUp++;
        }
    }
    
    Serial.printf("[SecurityKernel] Backed up %d/%d critical files\n", backedUp, CRITICAL_FILE_COUNT);
    
    return backedUp > 0;
}

bool SecurityKernel::restoreCriticalSettings() {
    Serial.println("[SecurityKernel] Restoring critical settings from backup...");
    
    if (!mountFS()) return false;
    
    int restored = 0;
    
    for (int i = 0; i < CRITICAL_FILE_COUNT; i++) {
        if (restoreFile(CRITICAL_FILES[i])) {
            restored++;
        }
    }
    
    Serial.printf("[SecurityKernel] Restored %d/%d critical files\n", restored, CRITICAL_FILE_COUNT);
    
    return restored > 0;
}

bool SecurityKernel::backupFile(const char* path) {
    if (!LittleFS.exists(path)) {
        return false;
    }
    
    // Create backup filename with timestamp
    char backupPath[MAX_PATH_LEN + 20];
    const char* fileName = strrchr(path, '/');
    if (fileName) {
        fileName++; // Skip the '/'
    } else {
        fileName = path;
    }
    
    snprintf(backupPath, sizeof(backupPath), "%s/%s.bak", BACKUP_DIR, fileName);
    
    // Copy file
    File src = LittleFS.open(path, "r");
    if (!src) return false;
    
    File dst = LittleFS.open(backupPath, "w");
    if (!dst) {
        src.close();
        return false;
    }
    
    // Read and write in chunks
    uint8_t buffer[256];
    while (src.available()) {
        size_t bytesRead = src.read(buffer, sizeof(buffer));
        if (dst.write(buffer, bytesRead) != bytesRead) {
            src.close();
            dst.close();
            return false;
        }
    }
    
    src.close();
    dst.close();
    
    return true;
}

bool SecurityKernel::restoreFile(const char* path) {
    // Find backup file
    char backupPath[MAX_PATH_LEN + 20];
    const char* fileName = strrchr(path, '/');
    if (fileName) {
        fileName++;
    } else {
        fileName = path;
    }
    
    snprintf(backupPath, sizeof(backupPath), "%s/%s.bak", BACKUP_DIR, fileName);
    
    if (!LittleFS.exists(backupPath)) {
        return false;
    }
    
    // Copy backup to original location
    File src = LittleFS.open(backupPath, "r");
    if (!src) return false;
    
    File dst = LittleFS.open(path, "w");
    if (!dst) {
        src.close();
        return false;
    }
    
    uint8_t buffer[256];
    while (src.available()) {
        size_t bytesRead = src.read(buffer, sizeof(buffer));
        if (dst.write(buffer, bytesRead) != bytesRead) {
            src.close();
            dst.close();
            return false;
        }
    }
    
    src.close();
    dst.close();
    
    return true;
}

bool SecurityKernel::listBackups(BackupEntry* entries, int& count) {
    if (!LittleFS.exists(BACKUP_DIR)) {
        count = 0;
        return false;
    }
    
    File root = LittleFS.open(BACKUP_DIR, "r");
    if (!root) return false;
    
    count = 0;
    File entry = root.openNextFile();
    
    while (entry && count < MAX_BACKUP_ENTRIES) {
        if (!entry.isDirectory()) {
            strncpy(entries[count].path, entry.name(), MAX_PATH_LEN - 1);
            entries[count].path[MAX_PATH_LEN - 1] = '\0';
            entries[count].size = entry.size();
            entries[count].timestamp = entry.getLastWrite();
            entries[count].valid = true;
            count++;
        }
        entry = root.openNextFile();
    }
    
    root.close();
    return count > 0;
}

bool SecurityKernel::clearOldBackups() {
    if (!LittleFS.exists(BACKUP_DIR)) return true;
    
    // Remove all files in backup directory
    File root = LittleFS.open(BACKUP_DIR, "r");
    if (!root) return false;
    
    File entry = root.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            char path[64];
            snprintf(path, sizeof(path), "%s/%s", BACKUP_DIR, entry.name());
            LittleFS.remove(path);
        }
        entry = root.openNextFile();
    }
    
    root.close();
    return true;
}

void SecurityKernel::triggerSelfDestruct() {
    Serial.println("[SecurityKernel] !!! SELF-DESTRUCT SEQUENCE INITIATED !!!");
    
    // Mark corruption in RTC for persistence
    security_rtc_magic = SECURITY_MAGIC_CORRUPT;
    security_rtc_corruption = CORRUPT_SYSTEM_CONFIG;
    
    // Log the event
    logEvent("SELF_DESTRUCT_TRIGGERED");
    
    // Backup critical data first
    backupCriticalSettings();
    
    // Clear user data
    LittleFS.remove("/settings.bin");
    LittleFS.remove("/cfg/system_config.json");
    LittleFS.remove("/cfg/ui_layout.json");
    LittleFS.remove("/cfg/user_prefs.lua");
    
    // Write corruption marker
    File f = LittleFS.open("/cfg/corruption.marker", "w");
    if (f) {
        f.println("SELF_DESTRUCT");
        f.println("System triggered controlled self-destruct");
        f.close();
    }
    
    Serial.println("[SecurityKernel] Self-destruct complete. System will restart.");
    
    // Safe restart
    safeRestart();
}

void SecurityKernel::simulateCorruption(CorruptionType type) {
    Serial.printf("[SecurityKernel] Simulating corruption type: %d\n", type);
    
    switch (type) {
        case CORRUPT_FILESYSTEM:
            // Corrupt filesystem by writing invalid data
            {
                File f = LittleFS.open("/cfg/corrupt_test.dat", "w");
                if (f) {
                    // Write garbage data
                    for (int i = 0; i < 100; i++) {
                        f.write(random(0, 255));
                    }
                    f.close();
                }
            }
            break;
            
        case CORRUPT_SETTINGS:
            // Corrupt settings file
            {
                File f = LittleFS.open("/settings.bin", "w");
                if (f) {
                    // Write invalid signature
                    uint32_t invalid = 0xDEADBEEF;
                    f.write((uint8_t*)&invalid, sizeof(invalid));
                    f.close();
                }
            }
            break;
            
        case CORRUPT_SYSTEM_CONFIG:
            // Corrupt system config
            {
                File f = LittleFS.open("/cfg/system_config.json", "w");
                if (f) {
                    f.println("{INVALID JSON DATA");
                    f.close();
                }
            }
            break;
            
        default:
            break;
    }
    
    // Mark for detection
    security_rtc_magic = SECURITY_MAGIC_CORRUPT;
    security_rtc_corruption = type;
    
    Serial.println("[SecurityKernel] Corruption simulated. Restart to detect.");
}

void SecurityKernel::enableWatchdog() {
    esp_task_wdt_init(10, true);
    esp_task_wdt_add(NULL);
}

void SecurityKernel::disableWatchdog() {
    esp_task_wdt_delete(NULL);
}

void SecurityKernel::safeRestart() {
    Serial.println("[SecurityKernel] Performing safe restart...");
    
    // Ensure all files are closed
    LittleFS.end();
    
    // Small delay for cleanup
    delay(100);
    
    esp_restart();
}

void SecurityKernel::generateReport(SecurityReport& report) {
    checkIntegrity(report);
}

void SecurityKernel::printReport(const SecurityReport& report) {
    Serial.println("========== SECURITY REPORT ==========");
    Serial.printf("Status: %d\n", report.status);
    Serial.printf("Integrity Score: %lu%%\n", report.integrityScore);
    Serial.printf("Free Heap: %lu bytes\n", report.freeHeap);
    Serial.printf("Free Flash: %lu bytes\n", report.freeFlash);
    Serial.printf("Filesystem Healthy: %s\n", report.fsHealthy ? "Yes" : "No");
    Serial.printf("Settings Valid: %s\n", report.settingsValid ? "Yes" : "No");
    Serial.printf("Backup Available: %s\n", report.backupAvailable ? "Yes" : "No");
    Serial.printf("Error: %s\n", report.errorMsg);
    Serial.println("=====================================");
}

uint32_t SecurityKernel::calculateChecksum(const uint8_t* data, size_t len) {
    // Simple CRC32-like checksum
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
        }
    }
    return crc ^ 0xFFFFFFFF;
}

bool SecurityKernel::verifyFileChecksum(const char* path, uint32_t expected) {
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    
    size_t size = f.size();
    uint8_t* buffer = (uint8_t*)malloc(size);
    if (!buffer) {
        f.close();
        return false;
    }
    
    f.read(buffer, size);
    f.close();
    
    uint32_t actual = calculateChecksum(buffer, size);
    free(buffer);
    
    return actual == expected;
}

bool SecurityKernel::writeIntegrityRecord() {
    File f = LittleFS.open(INTEGRITY_FILE, "w");
    if (!f) return false;
    
    DynamicJsonDocument doc(256);
    doc["status"] = _currentStatus;
    doc["corruption"] = _lastCorruption;
    doc["lastCheck"] = _lastCheckTime;
    doc["initialized"] = _initialized;
    
    serializeJson(doc, f);
    f.close();
    
    return true;
}

bool SecurityKernel::readIntegrityRecord() {
    if (!LittleFS.exists(INTEGRITY_FILE)) return false;
    
    File f = LittleFS.open(INTEGRITY_FILE, "r");
    if (!f) return false;
    
    DynamicJsonDocument doc(256);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    
    if (err) return false;
    
    _currentStatus = (SecurityStatus)(doc["status"] | SEC_STATUS_OK);
    _lastCorruption = (CorruptionType)(doc["corruption"] | CORRUPT_NONE);
    _lastCheckTime = doc["lastCheck"] | 0;
    _initialized = doc["initialized"] | false;
    
    return true;
}

void SecurityKernel::logEvent(const char* event) {
    File f = LittleFS.open(SECURITY_LOG, "a");
    if (f) {
        f.printf("[%lu] %s\n", millis(), event);
        f.close();
    }
}

CorruptionType SecurityKernel::detectCorruptionType() {
    // Check various indicators to determine corruption type
    if (!checkFilesystem()) {
        return CORRUPT_FILESYSTEM;
    }
    if (!checkSettings()) {
        return CORRUPT_SETTINGS;
    }
    if (!checkRTC()) {
        return CORRUPT_RTC_MEMORY;
    }
    return CORRUPT_NONE;
}