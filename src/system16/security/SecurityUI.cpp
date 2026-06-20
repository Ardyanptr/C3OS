#include "SecurityKernel.h"
#include "config/config.h"
#include <LittleFS.h>
#include <esp_task_wdt.h>

// Externs for display and buttons from C3OS
extern C3OSDisplay display;
extern OneButton btnUp;
extern OneButton btnDown;
extern OneButton btnOK;
extern OneButton btnAction;
extern void drawMenu();

// Security Kernel UI for C3OS
// Provides user interface for security diagnostics and recovery

static int menuIndex = 0;
static bool isRunning = true;

static const char* securityMenuItems[] = {
    "1. Check Integrity",
    "2. View Security Report",
    "3. Backup Settings",
    "4. Restore Settings",
    "5. Run Auto-Recovery",
    "6. Simulate Corruption",
    "7. Trigger Self-Destruct",
    "8. Exit"
};

static const int MENU_COUNT = 8;

static void drawSecurityUI(const char* title, const char* content[], int count, int selected) {
    display.clearBuffer();
    
    // Top Bar
    display.setDrawColor(1);
    display.drawBox(0, 0, 128, 11);
    display.setDrawColor(0);
    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(3, 9, title);
    display.setDrawColor(1);

    // Content
    display.setFont(u8g2_font_4x6_tr);
    for (int i = 0; i < count; i++) {
        int y = 18 + (i * 6);
        if (i == selected) {
            display.drawBox(0, y - 5, 128, 6);
            display.setDrawColor(0);
        } else {
            display.setDrawColor(1);
        }
        display.drawStr(3, y, content[i]);
    }
    display.setDrawColor(1);
    display.sendBuffer();
}

static void showSecurityReport() {
    SecurityReport report;
    SecurityKernel::generateReport(report);
    
    // Display report on screen
    display.clearBuffer();
    display.setDrawColor(1);
    display.drawBox(0, 0, 128, 11);
    display.setDrawColor(0);
    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(3, 9, "SECURITY REPORT");
    display.setDrawColor(1);
    
    display.setFont(u8g2_font_4x6_tr);
    
    char buf[32];
    int y = 18;
    
    snprintf(buf, sizeof(buf), "Score: %lu%%", report.integrityScore);
    display.drawStr(3, y, buf); y += 8;
    
    snprintf(buf, sizeof(buf), "Heap: %luB", report.freeHeap);
    display.drawStr(3, y, buf); y += 8;
    
    snprintf(buf, sizeof(buf), "FS: %s", report.fsHealthy ? "OK" : "ERR");
    display.drawStr(3, y, buf); y += 8;
    
    snprintf(buf, sizeof(buf), "Set: %s", report.settingsValid ? "OK" : "ERR");
    display.drawStr(3, y, buf); y += 8;
    
    snprintf(buf, sizeof(buf), "Bkup: %s", report.backupAvailable ? "Yes" : "No");
    display.drawStr(3, y, buf);
    
    display.sendBuffer();
    
    // Also print to serial
    SecurityKernel::printReport(report);
    
    // Wait for button press
    while (true) {
        esp_task_wdt_reset();
        if (digitalRead(BUTTON_OK) == LOW) break;
        delay(50);
    }
    while (digitalRead(BUTTON_OK) == LOW) { delay(10); esp_task_wdt_reset(); }
}

static void showCheckIntegrity() {
    display.clearBuffer();
    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(10, 30, "Checking...");
    display.sendBuffer();
    
    SecurityStatus status = SecurityKernel::checkIntegrity();
    
    display.clearBuffer();
    display.drawStr(10, 20, "Integrity Check");
    
    switch (status) {
        case SEC_STATUS_OK:
            display.drawStr(10, 35, "System Healthy!");
            break;
        case SEC_STATUS_WARNING:
            display.drawStr(10, 35, "Warnings Found");
            break;
        case SEC_STATUS_CRITICAL:
            display.drawStr(10, 35, "CRITICAL!");
            break;
        case SEC_STATUS_RECOVERED:
            display.drawStr(10, 35, "Recovered!");
            break;
        default:
            display.drawStr(10, 35, "Unknown");
            break;
    }
    
    display.sendBuffer();
    
    while (true) {
        esp_task_wdt_reset();
        if (digitalRead(BUTTON_OK) == LOW) break;
        delay(50);
    }
    while (digitalRead(BUTTON_OK) == LOW) { delay(10); esp_task_wdt_reset(); }
}

static void showBackupProgress() {
    display.clearBuffer();
    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(10, 30, "Backing up...");
    display.sendBuffer();
    
    bool success = SecurityKernel::backupCriticalSettings();
    
    display.clearBuffer();
    display.drawStr(10, 25, success ? "Backup Complete!" : "Backup Failed!");
    display.sendBuffer();
    
    while (true) {
        esp_task_wdt_reset();
        if (digitalRead(BUTTON_OK) == LOW) break;
        delay(50);
    }
    while (digitalRead(BUTTON_OK) == LOW) { delay(10); esp_task_wdt_reset(); }
}

static void showRestoreProgress() {
    display.clearBuffer();
    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(10, 30, "Restoring...");
    display.sendBuffer();
    
    bool success = SecurityKernel::restoreCriticalSettings();
    
    display.clearBuffer();
    display.drawStr(10, 25, success ? "Restore Complete!" : "Restore Failed!");
    display.sendBuffer();
    
    while (true) {
        esp_task_wdt_reset();
        if (digitalRead(BUTTON_OK) == LOW) break;
        delay(50);
    }
    while (digitalRead(BUTTON_OK) == LOW) { delay(10); esp_task_wdt_reset(); }
}

static void showRecoveryProgress() {
    display.clearBuffer();
    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(10, 30, "Recovering...");
    display.sendBuffer();
    
    bool success = SecurityKernel::attemptRecovery();
    
    display.clearBuffer();
    display.drawStr(10, 25, success ? "Recovery OK!" : "Recovery Failed!");
    display.sendBuffer();
    
    while (true) {
        esp_task_wdt_reset();
        if (digitalRead(BUTTON_OK) == LOW) break;
        delay(50);
    }
    while (digitalRead(BUTTON_OK) == LOW) { delay(10); esp_task_wdt_reset(); }
}

static void showSimulateCorruption() {
    display.clearBuffer();
    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(5, 20, "Simulate Corruption");
    display.drawStr(5, 30, "This will corrupt");
    display.drawStr(5, 40, "system data!");
    display.drawStr(5, 50, "OK=Confirm UP=Cancel");
    display.sendBuffer();
    
    while (true) {
        esp_task_wdt_reset();
        if (digitalRead(BUTTON_OK) == LOW) {
            SecurityKernel::simulateCorruption(CORRUPT_SETTINGS);
            display.clearBuffer();
            display.drawStr(10, 30, "Corrupted!");
            display.sendBuffer();
            delay(1000);
            break;
        }
        if (digitalRead(BUTTON_UP) == LOW) break;
        delay(50);
    }
    while (digitalRead(BUTTON_OK) == LOW || digitalRead(BUTTON_UP) == LOW) { 
        delay(10); 
        esp_task_wdt_reset(); 
    }
}

static void showSelfDestructWarning() {
    display.clearBuffer();
    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(5, 15, "!!! WARNING !!!");
    display.drawStr(5, 25, "Self-Destruct will:");
    display.drawStr(5, 35, "- Backup settings");
    display.drawStr(5, 45, "- Wipe user data");
    display.drawStr(5, 55, "- Restart system");
    display.drawStr(5, 62, "OK=Confirm UP=Cancel");
    display.sendBuffer();
    
    while (true) {
        esp_task_wdt_reset();
        if (digitalRead(BUTTON_OK) == LOW) {
            SecurityKernel::triggerSelfDestruct();
            // Should not return
            break;
        }
        if (digitalRead(BUTTON_UP) == LOW) break;
        delay(50);
    }
    while (digitalRead(BUTTON_OK) == LOW || digitalRead(BUTTON_UP) == LOW) { 
        delay(10); 
        esp_task_wdt_reset(); 
    }
}

void runSecurityManager() {
    isRunning = true;
    menuIndex = 0;
    
    // Initialize Security Kernel
    SecurityKernel::begin();

    btnUp.attachClick([]() {
        menuIndex = (menuIndex - 1 + MENU_COUNT) % MENU_COUNT;
    });

    btnDown.attachClick([]() {
        menuIndex = (menuIndex + 1) % MENU_COUNT;
    });

    btnOK.attachClick([]() {
        switch(menuIndex) {
            case 0: showCheckIntegrity(); break;
            case 1: showSecurityReport(); break;
            case 2: showBackupProgress(); break;
            case 3: showRestoreProgress(); break;
            case 4: showRecoveryProgress(); break;
            case 5: showSimulateCorruption(); break;
            case 6: showSelfDestructWarning(); break;
            case 7: isRunning = false; break;
        }
    });

    while(isRunning) {
        esp_task_wdt_reset();
        
        drawSecurityUI("SECURITY KERNEL", securityMenuItems, MENU_COUNT, menuIndex);
        
        btnUp.tick();
        btnDown.tick();
        btnOK.tick();
        
        delay(20);
    }

    drawMenu();
}