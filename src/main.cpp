#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <rom/rtc.h>

#include "UI/floater.h"
#include "app/Essential/Settings.h"
#include "component/liveness.h"
#include "config/config.h"
#include "config/var_declare.h"
#include "esp_task_wdt.h"
#include "include/CoreLib.h"
#include "include/EssentialAppLib.h"
#include "include/SystemCoreLib.h"
#include "include/WiFiAppLib.h"
#include "system16/recovery/SystemRescue.h"
#include "system16/boot/boot.h"
#include "system16/boot/bootloader.h"
#include "system16/crash_handler.h"
#include "system16/loader.h"
#include "system16/power.h"
#include "system16/state_manager.h"
#include "system16/update.h"
#include "system16/security/SecurityKernel.h"
#include "component/process_manager.h"
#include "UI/task_manager.h"
#include "app/CrashApp/CrashEngine.h"

#include "esp_console.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"

// OS Variables and Stuffs
volatile uint32_t last_heartbeat = 0;
hw_timer_t* watchdog_timer = NULL;

extern "C" void esp_task_wdt_isr_user_handler(void) {
    crash_magic = CRASH_MAGIC_PENDING;
    crash_time = millis();

    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    crash_task = (uint32_t)task;
}

void IRAM_ATTR watchdog_isr() {
    uint32_t now = millis();
    uint32_t elapsed = now - last_heartbeat;

    if (elapsed > HEARTBEAT_TIMEOUT_MS && last_heartbeat != 0) {
        crash_magic = CRASH_MAGIC_PENDING;
        crash_time = now;
        crash_task = 0xBADC0DE;

        esp_restart();
    }
}

// Board Info
String BOARD_ATTACHED = "ESP32-C3"; // fixed
int BOARD_REV = 4;

// Start of Initializer
C3OSDisplay display(U8G2_R0, U8X8_PIN_NONE);

OneButton btnUp(BUTTON_UP, true);
OneButton btnDown(BUTTON_DOWN, true);
OneButton btnOK(BUTTON_OK, true);
OneButton btnAction(BUTTON_ACTION, true);

AT24C256 eep;

Settings setting;
PowerManager gPower;
DFRobot_VL53L0X sensor;
// End of Initializer

uint32_t appLastBeat = 0;
uint8_t anrSelect = 0;

bool appRunning = false;
bool anrActive = false;
bool systemUIActive = true;

int currentApp = -1;
int anrApp = -1;

unsigned long longPressScrollLast = 0;
int longPRessScrollInterval = 150;
int longPressScrollCount = 0;
int longPressUpActive = false;
int longPressDownActive = false;

RTC_DATA_ATTR boot_mode_t boot_mode = BOOT_NORMAL;
uint32_t SLEEP_TIMEOUT;

void showLockscreen(bool isWake);
void drawMenu();
void boot();

float currentBoxY = 0;
const int ITEM_HEIGHT = 16;

bool isOnErrorFlag = true;
volatile uint32_t lastTick_wd;

void appHeartBeat() {
    appLastBeat = millis();

    TaskHandle_t current = xTaskGetCurrentTaskHandle();
    auto& procs = ProcessManager::instance().getProcesses();
    for (auto& p : procs) {
        if (p.handle == current) {
            int fgId = ProcessManager::instance().getForegroundId();
            if (p.id != fgId) {
                Serial.printf("[PROC] Suspending background task %s (ID: %d)\n", p.name, p.id);
                p.state = PROC_BACKGROUND;
                
                vTaskSuspend(NULL);

                Serial.printf("[PROC] Resumed task %s (ID: %d)\n", p.name, p.id);
                esp_task_wdt_reset();
                appLastBeat = millis();
            }
            break;
        }
    }
}

void runSettings() {
    setting.begin();
    setting.run();
}

void check_sys_integrity_run() { check_sys_integrity(); }

void non_set() {}

void kernelSweep() {
    static uint32_t last = 0;
    if (millis() - last < 5000) return;
    last = millis();

    MEM::checkHeapTrend();
}

void ui_watchdog(void* arg) {
    vTaskDelay(pdMS_TO_TICKS(1000));

    while (1) {
        uint32_t elapsed = millis() - lastTick_wd;

        if (elapsed > 20000) {
            crash_magic = CRASH_MAGIC_PENDING;
            crash_time = millis();
            crash_task = (uint32_t)xTaskGetCurrentTaskHandle();

            vTaskDelay(pdMS_TO_TICKS(5000));
            esp_restart();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void enable_pm() {
    esp_pm_config_esp32c3_t pm = {
        .max_freq_mhz = 160, .min_freq_mhz = 40, .light_sleep_enable = false};

    esp_pm_configure(&pm);
}

unsigned long lastActive = 0;
static unsigned long lastTick = 0;
bool isOnSleepMode = false;
bool pendingLockscreen = false;

const char* menuItems[] = {
    "WiFi Scan",     "WiFi Connect",    "Settings",       "Brightness",    "Browser",
    "WiFi Sniffer",  "Beacon Spammer",  "Pong",           "Hotspot",       "Ruler",
    "Keyboard Cat",  "WiFi Storm",      "Echo Sniffer",   "IR Blaster",    "Flappy Bird",
    "Factory Reset", "Check Integrity", "Online Story",   "Heap Monitor",  "Stopwatch",
    "BenchMyESP",    "Minecraft",       "Stardew Valley", "BLE Scan",      "C3 Burner",
    "WiFi Telnet",   "Doom GL",         "File Manager",   "Store Manager", "Timer",
    "Update",        "Radar Maker",     "Prism Launcher", "Crash Manager"};

const int menuCount = sizeof(menuItems) / sizeof(menuItems[0]);
const int visibleItem = sysConfig.visible_items;
const int NOTIFICATION_DURATION = 2000;

unsigned long scrollStartTime = 0;
unsigned long notificationStart = 0;

int SCROLL_DURATION = sysConfig.scroll_duration;
int menuIndex = 0;
int menuScrollOffset = 0;
int targetMenuIndex = 0;
int targetScrollOffset = 0;

bool isScrolling = false;
bool notificationActive = false;
bool isEjected = false;

String notificationText = "";
int lastSelectedIndex = -1;

void IRAM_ATTR onButton() { lastActive = millis(); }

void initMenuButton();
void showFinalErrorScreen();
void showSuccessScreen();

// Game engine
void runGameClient_SWT() {
    GameClient* client = new GameClient(BUTTON_UP, BUTTON_DOWN, BUTTON_ACTION, BUTTON_OK);

    client->begin();
    client->runGame();

    delete client;
}

void runGameClient_SVC() {
    GameClient_svc client(BUTTON_UP, BUTTON_DOWN, BUTTON_ACTION, BUTTON_OK);

    client.begin();
    client.runGame();
}

void runGameClient_DOOM() {
    GameClient_Doom client(BUTTON_UP, BUTTON_DOWN, BUTTON_OK, BUTTON_ACTION);

    client.begin();
    client.runGame();
}

void showShortcutGUI() {
    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    display.drawRFrame(2, 1, 124, 52, 3);

    display.setFont(u8g2_font_5x7_tr);
    display.drawStr(6, 11, "DFU Mode");
    display.drawStr(6, 19, "Lockscreen");

    display.drawStr(9, 62, "1: 1, 2: 2, 3: 3, L: 4");
    display.sendBuffer();

    detachCallback();

    btnOK.attachClick([]() {
        display.clearBuffer();

        display.setFontMode(1);
        display.setBitmapMode(1);

        display.drawBox(0, 32, 55, 2);

        display.drawRFrame(54, 28, 18, 10, 3);
        display.drawRBox(54, 28, 13, 10, 1);

        display.setFont(u8g2_font_4x6_tr);
        display.drawStr(34, 50, "Upload firmware");
        display.drawStr(42, 57, "to proceed.");

        display.sendBuffer();

        while (true) {
            esp_task_wdt_reset();
            vTaskDelay(5);
            yield();
        }

        initMenuButton();
        drawMenu();
    });
    btnOK.attachDoubleClick([]() {
        display.clearBuffer();

        showLockscreen(true);

        initMenuButton();
        drawMenu();
    });
}

void syncESP8266() {
    display.clearBuffer();

    draw_restartESP8266();
    setupESP8266Communication();
    esp_task_wdt_reset();

    sendCommand("32:start");
    delay(100);
    esp_task_wdt_reset();

    sendCommand("avr32:force-restart");
    delay(100);
    esp_task_wdt_reset();

    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    display.drawXBM(29, 28, 7, 8, image_Rpc_active_bits);

    display.setFont(u8g2_font_5x7_tr);
    display.drawStr(41, 35, "Restarting MCU");
    display.sendBuffer();

    setupESP8266Communication();
    esp_task_wdt_reset();

    sendCommand("32:start");
    delay(100);
    esp_task_wdt_reset();

    sendCommand("avr32:test");
    esp_task_wdt_reset();

    delay(500);
    esp_task_wdt_reset();

    drawMenu();
}

void showActionGUI() {
    display.clearBuffer();
    display.sendBuffer();

    display.setBitmapMode(1);

    display.drawRFrame(31, 14, 67, 36, 5);
    display.drawXBM(34, 17, 19, 20, image_power_bits);
    display.drawXBM(55, 17, 19, 20, image_play_bits);
    display.drawXBM(76, 17, 19, 20, image_off_bits);

    display.drawXBM(36, 39, 10, 8, image_Pin_back_arrow_bits);

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(49, 46, "Back");
    display.sendBuffer();

    btnAction.attachClick(shutdown_now);
    btnAction.attachDoubleClick(restart_now);
    btnAction.attachMultiClick(syncESP8266);

    btnAction.attachLongPressStart([]() {
        powerNotifyActivity();
        lastActive = millis();
        gPower.notifyActivity();
        
        // Wait for button release
        while(digitalRead(BUTTON_ACTION) == LOW) { delay(10); esp_task_wdt_reset(); }

        initMenuButton();
        drawMenu();
    });
}

float menuYOffset = 0.0f;
bool menuEntranceRequested = false;

void drawMenuItemsWithOffset(float scrollOffset) {
    SCROLL_DURATION = sysConfig.scroll_duration;
    const int textOffsetY = sysConfig.text_offset_y;
    const int drawCount = sysConfig.draw_count;

    int intOffset = (int)floor(scrollOffset);
    float fractionalOffset = scrollOffset - (float)intOffset;

    float targetBoxY = (menuIndex - scrollOffset) * ITEM_HEIGHT;

    int UI_OFFSET_Y = 10 + (int)menuYOffset;

    !isScrolling ? currentBoxY = targetBoxY : currentBoxY += (targetBoxY - currentBoxY) * 0.4f;

    display.setDrawColor(1);
    display.drawRBox(0, (int)currentBoxY + UI_OFFSET_Y, 128, ITEM_HEIGHT, 3);

    if (systemUIActive) Liveness::Effects::drawFocusPulse(display, (int)currentBoxY + UI_OFFSET_Y, ITEM_HEIGHT);

    const int baseTextX = 4;

    for (int i = 0; i < drawCount; i++) {
        int itemIndex = i + intOffset;
        if (itemIndex < 0 || itemIndex >= menuCount) continue;

        int yPos = (int)((i * ITEM_HEIGHT) - (fractionalOffset * ITEM_HEIGHT)) + UI_OFFSET_Y;
        if (yPos < -ITEM_HEIGHT + UI_OFFSET_Y || yPos > 64) continue;

        bool focused = (itemIndex == menuIndex);

        display.setDrawColor(focused ? 0 : 1);

        int textX = baseTextX + (focused ? 6 : 0);
        display.drawStr(textX, yPos + textOffsetY, menuItems[itemIndex]);
    }
}

bool skipButtonInit = false;

void drawMenu() {
    appRunning = false;
    currentApp = -1;
    anrActive = sysConfig.anr_active;
    systemUIActive = sysConfig.system_ui_active;

    if (menuEntranceRequested) {
        menuYOffset = 64.0f;
        menuEntranceRequested = false;
    }
    menuYOffset += (0.0f - menuYOffset) * 0.15f;

    display.setFont(u8g2_font_7x14_tr);
    display.clearBuffer();

    if (systemUIActive) Liveness::Ambient::updateAndDraw(display);
    if (!skipButtonInit) initMenuButton();

    float currentOffset;
    if (isScrolling) {
        unsigned long currentTime = millis();
        float progress = (currentTime - scrollStartTime) / (float)SCROLL_DURATION;

        if (progress >= 1.0f) {
            menuScrollOffset = (float)targetScrollOffset;
            isScrolling = false;
            currentOffset = menuScrollOffset;
        } else {
            float easedProgress = easeOutBack(progress);
            currentOffset =
                menuScrollOffset + ((float)targetScrollOffset - menuScrollOffset) * easedProgress;
        }
    } else {
        currentOffset = (float)targetScrollOffset;
        menuScrollOffset = (float)targetScrollOffset;
    }

    drawMenuItemsWithOffset(currentOffset);

    if (systemUIActive) {
        Liveness::StatusBar::draw(display);
        Liveness::Notifications::draw(display);
    }

    display.setDrawColor(1);
    display.sendBuffer();

    static bool needsFadeIn = false;
    if (menuYOffset > 60.0f) needsFadeIn = true;
    
    if (needsFadeIn) {
        UX::TransitionEffects::fadeIn();
        needsFadeIn = false;
    }
}

void updateScroll() {
    StateManager::saveState(menuIndex, -1, false);
    const int visibleItemsCount = 4;
    int newScrollOffset = targetScrollOffset;

    if (menuIndex < newScrollOffset) {
        newScrollOffset = menuIndex;
    } else if (menuIndex >= newScrollOffset + visibleItemsCount) {
        newScrollOffset = menuIndex - visibleItemsCount + 1;
    }

    if (newScrollOffset != targetScrollOffset) {
        if (isScrolling) {
            float progress = (millis() - scrollStartTime) / (float)SCROLL_DURATION;
            if (progress > 1.0f) progress = 1.0f;
            menuScrollOffset = menuScrollOffset + ((float)targetScrollOffset - menuScrollOffset) *
                                                      easeOutCubic(progress);
        } else {
            menuScrollOffset = (float)targetScrollOffset;
        }

        targetScrollOffset = newScrollOffset;
        isScrolling = true;
        scrollStartTime = millis();
    }
}

static void animateLaunch(const char* name) {
    for (int i = Settings::instance->get().oledContrast; i >= 0; i -= 15) {
        display.setContrast(i);
        delay(10);
        esp_task_wdt_reset();
    }

    display.clearBuffer();
    display.setFont(u8g2_font_7x14_tr);

    int width = display.getStrWidth(name);

    display.drawStr((128 - width) / 2, 35, name);
    display.sendBuffer();

    for (int i = 0; i <= Settings::instance->get().oledContrast; i += 15) {
        display.setContrast(i);
        delay(10);
        esp_task_wdt_reset();
    }

    delay(300);

    for (int i = Settings::instance->get().oledContrast; i >= 0; i -= 15) {
        display.setContrast(i);
        delay(10);
        esp_task_wdt_reset();
    }
}

static void animateExit(const char* name) {
    for (int i = Settings::instance->get().oledContrast; i >= 0; i -= 15) {
        display.setContrast(i);
        delay(10);
        esp_task_wdt_reset();
    }
}

void runApp(int index) {
    if (index < 0 || index >= menuCount) return;

    AppDesc& app = appTable[index];
    if (ESP.getFreeHeap() < app.minHeap) return;

    StateManager::saveState(menuIndex, index, true);
    animateLaunch(app.name);

    Liveness::Notifications::show("Launching: " + String(app.name));

    // Launch via ProcessManager
    int pid = ProcessManager::instance().launchApp(index);
    if (pid == -1) {
        Liveness::Notifications::show("Launch Failed");
        animateExit(app.name);
        drawMenu();
        return;
    }

    appRunning = true;
    currentApp = index;

    while (ProcessManager::instance().getForegroundId() == pid) {
        btnUp.tick();
        btnDown.tick();
        btnOK.tick();
        btnAction.tick();
        
        if (digitalRead(BUTTON_ACTION) == LOW) {
            unsigned long start = millis();
            while(digitalRead(BUTTON_ACTION) == LOW) {
                if (millis() - start > 1000) {
                    ProcessManager::instance().setForeground(-1);
                    break;
                }
                delay(10);
                esp_task_wdt_reset();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
        esp_task_wdt_reset();
        last_heartbeat = millis();
    }

    appRunning = false;
    currentApp = -1;

    animateExit(app.name);
    Liveness::Notifications::show("App Backgrounded");

    StateManager::saveState(menuIndex, -1, false);

    menuEntranceRequested = true;
    drawMenu();
}

void handleLongPressScroll() {
    esp_task_wdt_reset();
    unsigned long now = millis();

    if (!longPressUpActive && !longPressDownActive) return;
    if (now - longPressScrollLast < (unsigned long)longPRessScrollInterval) return;

    longPressScrollLast = now;
    longPressScrollCount++;

    if (longPressScrollCount % 5 == 0) longPRessScrollInterval = max(40, longPRessScrollInterval - 20);
        
    longPressUpActive ? menuIndex = (menuIndex - 1 + menuCount) % menuCount : menuIndex = (menuIndex + 1) % menuCount; 

    updateScroll();
    drawMenu();
}

void initMenuButton() {
    detachCallback();

    btnUp.attachClick([]() {
        powerNotifyActivity();
        lastActive = millis();
        gPower.notifyActivity();
        menuIndex = (menuIndex - 1 + menuCount) % menuCount;
        updateScroll();
        drawMenu();
    });

    btnDown.attachClick([]() {
        powerNotifyActivity();
        lastActive = millis();
        gPower.notifyActivity();
        menuIndex = (menuIndex + 1) % menuCount;
        updateScroll();
        drawMenu();
    });

    btnUp.attachLongPressStart([]() {
        longPressUpActive = true;
        longPressScrollCount = 0;
        longPressScrollLast = millis();
    });

    btnUp.attachLongPressStop([]() {
        longPressUpActive = false;
        longPressScrollCount = 0;
        longPRessScrollInterval = 150;
    });

    btnDown.attachLongPressStart([]() {
        longPressDownActive = true;
        longPressScrollCount = 0;
        longPressScrollLast = millis();
    });

    btnDown.attachLongPressStop([]() {
        longPressDownActive = false;
        longPressScrollCount = 0;
        longPRessScrollInterval = 150;
    });

    btnOK.attachClick([]() {
        powerNotifyActivity();
        lastActive = millis();
        gPower.notifyActivity();

        runApp(menuIndex);
    });

    btnAction.attachClick([]() {
        powerNotifyActivity();
        lastActive = millis();
        gPower.notifyActivity();
        
        while(digitalRead(BUTTON_ACTION) == LOW) { delay(10); esp_task_wdt_reset(); }
        
        showActionGUI();
    });

    btnAction.attachLongPressStart([]() {
        powerNotifyActivity();
        lastActive = millis();
        gPower.notifyActivity();
        
        showTaskManager();
        
        initMenuButton();
        drawMenu();
    });
}

// C3OS Terminal RCPM
static int do_read_file(int argc, char **argv) {
    if(argc < 2) {
        printf("Use: read [filename]\n");
        return 1;
    }

    File f = LittleFS.open(argv[1], "r");
    if (!f) {
        printf("File %s not found", argv[1]);
        return 1;
    }
    while(f.available()) {
        putchar(f.read());
    }

    f.close();
    printf("\n");
    return 0;
}

// Setup

void setup() {
    Serial.begin(115200);
    Serial.println("Boot start");
    esp_reset_reason_t reason = esp_reset_reason();
    Serial.printf("Reset reason: %d\n", reason);

    delay(500);
    Wire.begin(SDA_PIN, SCL_PIN, 800000);
    Wire.setTimeOut(50);
    Wire.setClock(800000);
    
    eep.begin(); // Ensure EEPROM is initialized

    display.setBusClock(800000);
    i2cMutex = xSemaphoreCreateMutex();

    display.begin();
    display.setContrast(200);
    display.setFont(u8g2_font_6x10_tr);
    display.setBitmapMode(1);

    display.sendBuffer();

    lastTick_wd = millis();

    extern void runCrashManager();
    CrashEngine::begin();
    SecurityKernel::begin();
    
    if (CrashEngine::shouldEnterSafeMode()) runCrashManager();

    if (crash_magic == CRASH_MAGIC_PENDING) handle_crash_recovery();

    crash_magic = 0;

    Serial.println("I made to be here!");

    esp_err_t wdt_err = esp_task_wdt_add(NULL);
    if (wdt_err != ESP_OK && wdt_err != ESP_ERR_INVALID_ARG) {
        Serial.printf("[WDT] Warning: esp_task_wdt_add returned 0x%x\n", wdt_err); // ESP_ERR_INVALID_ARG means task already subscribed — that's fine
    }
    esp_task_wdt_reset();

    BootloaderUI bootloader(&display, &btnUp, &btnDown, &btnOK);
    BootMode selectedMode = bootloader.showMenu(3);

    switch (selectedMode) {
        case BL_BOOT_NORMAL:
            boot();
            Liveness::Notifications::show("C3OS Ready");
            break;
        case BL_BOOT_RECOVERY:
            SystemRescue::run(&display, &btnUp, &btnDown, &btnOK);
            esp_restart();
            break;
        case BL_BOOT_DFU:
            break;
        case BL_BOOT_RCPM: {
            Serial.println("\n\nStarting C3-DOS...");
            delay(500);

            display.clearBuffer();
            display.setFont(u8g2_font_6x10_tf);
            display.drawStr(0, 10, "C3-DOS Version 1.0");
            display.drawStr(0, 22, "Copyright (C) 2026");
            display.drawStr(0, 34, "LittleFS Mount: OK");
            display.drawStr(0, 46, "RAM Available: OK");
            display.drawStr(0, 58, "A:\\>"); 
            display.sendBuffer();

            Serial.println("C3-DOS Version 1.0");
            Serial.println("Copyright (C) 2026 Ardyan");
            Serial.print("A:\\>");

            String inputBuffer = "";

            while(true) {
                esp_task_wdt_reset();

                if (Serial.available() > 0) {
                    char ch = Serial.read();

                    if (ch == '\n' || ch == '\r') {
                        Serial.println();
                        inputBuffer.trim();

                        if (inputBuffer.length() > 0) {
                            if (inputBuffer.equalsIgnoreCase("ver")) {
                                Serial.println("C3-DOS [Version 1.0.25]");
                                Serial.println("Running on ESP32-C3 RISC-V Architecture");
                                
                                display.clearBuffer();
                                display.drawStr(0, 10, "A:\\> ver");
                                display.drawStr(0, 25, "C3-DOS v1.0");
                                display.drawStr(0, 40, "ESP32-C3 Mini OS");
                            }
                            else if (inputBuffer.equalsIgnoreCase("cls")) {
                                Serial.print("\e[H\e[J"); 
                                
                                display.clearBuffer();
                            }
                            else if (inputBuffer.equalsIgnoreCase("reboot")) {
                                display.clearBuffer();
                                display.drawStr(40, 35, "Rebooting...");
                                display.sendBuffer();
                                
                                Serial.println("Rebooting...");
                                esp_restart();
                                
                            }
                            else if (inputBuffer.equalsIgnoreCase("mem")) {
                                uint32_t freeHeap = ESP.getFreeHeap();
                                Serial.printf("   %d bytes total conventional memory\n", 400000);
                                Serial.printf("   %d bytes available to DOS\n", freeHeap);
                                
                                display.clearBuffer();
                                display.drawStr(0, 10, "A:\\> mem");
                                display.setCursor(0, 30);
                                display.print("Free RAM:");
                                display.setCursor(0, 45);
                                display.print(freeHeap);
                                display.print(" Bytes");
                            }
                            else if (inputBuffer.equalsIgnoreCase("dir")) {
                                Serial.println(" Volume in drive A has no label.");
                                Serial.println(" Directory of A:\\\n");
                                
                                display.clearBuffer();
                                display.drawStr(0, 10, "A:\\> dir");
                                int oledY = 25;

                                File root = LittleFS.open("/");
                                File file = root.openNextFile();
                                int fileCount = 0;
                                
                                while(file) {
                                    Serial.printf("%-12s\t%d bytes\n", file.name(), file.size());
                                    
                                    if (fileCount < 3) {
                                        display.setCursor(0, oledY);
                                        display.print(file.name());
                                        display.print(" ");
                                        display.print(file.size());
                                        oledY += 12;
                                    }
                                    fileCount++;
                                    file = root.openNextFile();
                                }
                                Serial.printf("\n\t%d File(s) listed.\n", fileCount);
                            }
                            else if (inputBuffer.startsWith("type ")) {
                                String filename = inputBuffer.substring(5);
                                filename.trim();
                                if(!filename.startsWith("/")) filename = "/" + filename;

                                File f = LittleFS.open(filename, "r");
                                if (f) {
                                    Serial.printf("Reading %s:\n---\n", filename.c_str());
                                    display.clearBuffer();
                                    display.setCursor(0, 10);
                                    display.print("Reading: "); display.print(filename);
                                    int oledY = 25;
                                    
                                    while(f.available()) {
                                        char fileChar = f.read();
                                        Serial.write(fileChar);
                                    }
                                    f.close();
                                    Serial.println("\n---");
                                } else {
                                    Serial.println("File not found.");
                                }
                            }
                            else if (inputBuffer.equalsIgnoreCase("help")) {
                                Serial.println("Supported commands:");
                                Serial.println("  DIR   - List files in LittleFS");
                                Serial.println("  TYPE  - Display contents of a file (e.g., type boot.txt)");
                                Serial.println("  MEM   - Check free memory/RAM");
                                Serial.println("  VER   - Display C3-DOS version");
                                Serial.println("  CLS   - Clear screen");
                                Serial.println("  EXIT  - Return to Main OS Menu");
                            }
                            else if (inputBuffer.equalsIgnoreCase("exit")) {
                                Serial.println("Exiting DOS environment...");
                                break;
                            }
                            else {
                                Serial.println("Bad command or file name");
                                
                                display.clearBuffer();
                                display.drawStr(0, 10, "Bad command");
                            }
                        }

                        Serial.print("\nA:\\>");
                        display.drawStr(0, 58, "A:\\>");
                        display.sendBuffer();
                        
                        inputBuffer = "";
                    } 
                    else if (ch == 8 || ch == 127) {
                        if (inputBuffer.length() > 0) {
                            inputBuffer.remove(inputBuffer.length() - 1);
                            Serial.print("\b \b");
                        }
                    } 
                    else {
                        inputBuffer += ch;
                        Serial.print(ch); 
                    }
                }
            }
        }
        break;
    }
}

void loop() {
    btnUp.tick();
    btnDown.tick();
    btnOK.tick();
    btnAction.tick();

    unsigned long now = millis();

    timerTick();

    anr_update_handler();
    ProcessManager::instance().update();
    if (anrActive) {
        drawANROverlay();
        return;
    }

    if (pendingLockscreen) {
        pendingLockscreen = false;
        showLockscreen(true);
    }

    gPower.loop();

    if (isScrolling || abs(menuYOffset) > 0.1f) {
        drawMenu();
    } else if (!appRunning && systemUIActive) {
        static unsigned long lastLivenessUpdate = 0;
        if (now - lastLivenessUpdate > 50) {
            skipButtonInit = true;
            drawMenu();
            skipButtonInit = false;
            lastLivenessUpdate = now;
        }
    }

    handleLongPressScroll();

    if (millis() - lastActive > SLEEP_TIMEOUT) {
        display.clearBuffer();
        enterSleep();
        lastActive = millis();
        isOnSleepMode = true;
    }

    if (WiFi.getMode() == WIFI_AP || WiFi.softAPgetStationNum() > 0) dnsServer.processNextRequest();

    server.handleClient();
    processSerialInput();

    lastTick_wd = millis();

    esp_task_wdt_reset();
    last_heartbeat = millis();

    delay(1);
}