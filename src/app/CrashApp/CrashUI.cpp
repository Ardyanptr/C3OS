#include "CrashEngine.h"
#include "config/config.h"
#include "component/service.h"

// Externs for display and buttons from C3OS
extern C3OSDisplay display;
extern OneButton btnUp;
extern OneButton btnDown;
extern OneButton btnOK;
extern OneButton btnAction;
extern void drawMenu();

static int menuIndex = 0;
static bool isRunning = true;

static const char* crashMenuItems[] = {
    "Simulate Mem Leak",
    "Simulate Null Ptr",
    "Simulate WDT TO",
    "Simulate Self-Destruct",
    "Core Dump: [ON]",
    "Clear Log",
    "Exit"
};

void drawCrashUI(const char* title, const char* content[], int count, int selected) {
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
        int y = 20 + (i * 8);
        if (i == selected) {
            display.drawBox(0, y - 6, 128, 8);
            display.setDrawColor(0);
        } else {
            display.setDrawColor(1);
        }
        display.drawStr(5, y, content[i]);
    }
    display.setDrawColor(1);
    display.sendBuffer();
}

void showSafeMode() {
    CrashLog log;
    if (!CrashEngine::loadLog(log)) {
        display.clearBuffer();
        display.drawStr(10, 30, "No Crash Log Found");
        display.sendBuffer();
        delay(2000);
        return;
    }

    const char* reasonStr = "Unknown";
    switch(log.resetReason) {
        case ESP_RST_PANIC: reasonStr = "Panic"; break;
        case ESP_RST_TASK_WDT: reasonStr = "Task WDT"; break;
        case ESP_RST_INT_WDT: reasonStr = "Int WDT"; break;
        case ESP_RST_BROWNOUT: reasonStr = "Brownout"; break;
        case ESP_RST_SW: reasonStr = "Software"; break;
    }

    char line1[32], line2[32], line3[32], line4[32];
    snprintf(line1, 32, "App: %s", log.appName);
    snprintf(line2, 32, "Heap: %u B", log.freeHeap);
    snprintf(line3, 32, "Reason: %s", reasonStr);
    snprintf(line4, 32, "Code: 0x%X", log.errorCode);

    const char* content[] = {line1, line2, line3, line4, "", "Press OK to Clear & Reboot"};
    
    while(true) {
        esp_task_wdt_reset();

        display.clearBuffer();
        display.setDrawColor(1);
        display.drawBox(0, 0, 128, 11);
        display.setDrawColor(0);
        display.drawStr(3, 9, "SAFE-MODE DIAGNOSTIC");
        display.setDrawColor(1);
        
        display.setFont(u8g2_font_4x6_tr);
        for(int i=0; i<6; i++) {
            display.drawStr(5, 20 + (i*8), content[i]);
        }
        display.sendBuffer();

        btnOK.tick();
        if (digitalRead(BUTTON_OK) == LOW) {
            CrashEngine::clearAndReboot();
        }
        delay(50);
    }
}

void runCrashManager() {
    isRunning = true;
    menuIndex = 0;
    const int MENU_COUNT = 7;

    // Safe boot check
    if (CrashEngine::shouldEnterSafeMode()) {
        showSafeMode();
    }

    btnUp.attachClick([]() {
        menuIndex = (menuIndex - 1 + MENU_COUNT) % MENU_COUNT;
    });

    btnDown.attachClick([]() {
        menuIndex = (menuIndex + 1) % MENU_COUNT;
    });

    btnOK.attachClick([]() {
        switch(menuIndex) {
            case 0: CrashEngine::simulateMemoryLeak(); break;
            case 1: CrashEngine::simulateNullPointer(); break;
            case 2: CrashEngine::simulateWatchdogTimeout(); break;
            case 3: CrashEngine::simulateSelfDestruct(); break;
            case 4: 
                CrashEngine::setCoreDumpEnabled(!CrashEngine::isCoreDumpEnabled());
                break;
            case 5: 
                CrashEngine::clearAndReboot();
                break;
            case 6: 
                isRunning = false;
                break;
        }
    });

    while(isRunning) {esp_task_wdt_reset();
        
        char dumpStatus[25];
        snprintf(dumpStatus, 25, "Core Dump: [%s]", CrashEngine::isCoreDumpEnabled() ? "ON" : "OFF");
        crashMenuItems[4] = dumpStatus;

        drawCrashUI("CRASH MANAGER", crashMenuItems, MENU_COUNT, menuIndex);
        
        btnUp.tick();
        btnDown.tick();
        btnOK.tick();
        
        delay(20);
    }

    drawMenu();
}
