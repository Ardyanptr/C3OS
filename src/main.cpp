#include <Arduino.h>
#include <ArduinoJson.h>

#include "LittleFS.h"
#include "config/config.h"
#include "config/var_declare.h"
#include "include/BLEAppLib.h"
#include "include/ComponentLib.h"
#include "include/CoreLib.h"
#include "include/EssentialAppLib.h"
#include "include/SystemCoreLib.h"
#include "include/UILib.h"
#include "include/WiFiAppLib.h"
#include "system16/loader.h"

String BOARD_ATTACHED = "ESP32-C3";
int BOARD_REV = 4;

extern "C" void esp_panic_handler() {
    Serial.println("System on Panic Mode!");
    panic(PANIC_UNKNOWN, "CPU exception");
}

extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) {
    panic(PANIC_STACK_OVERFLOW, pcTaskName);
}

extern "C" void vApplicationMallocFailedHook() {
    panic(PANIC_MEM_CORRUPT, "Heap malloc failed");
}

extern "C" void esp_task_wdt_isr_user_handler(void) {
    panic(PANIC_UNKNOWN, "WDT Timeout");
}

void __attribute__((weak)) esp_panic_handler(void);

void runGameClient_SWT() {
    GameClient client(BUTTON_UP, BUTTON_DOWN, BUTTON_ACTION, BUTTON_OK);

    client.begin();
    client.runGame();
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

U8G2_SSD1315_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

OneButton btnUp(BUTTON_UP, true);
OneButton btnDown(BUTTON_DOWN, true);
OneButton btnOK(BUTTON_OK, true);
OneButton btnAction(BUTTON_ACTION, true);

AT24C256 eep;

Settings setting;
PowerManager gPower;
DFRobot_VL53L0X sensor;

uint32_t appLastBeat = 0;
uint8_t anrSelect = 0;

bool appRunning = false;
bool anrActive = false;
bool systemUIActive = true;

int currentApp = -1;
int anrApp = -1;

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
}

void runSettings() {
    setting.begin();
    setting.run();
}

void factory_reset_run() {
}

void check_sys_integrity_run() {
    check_sys_integrity();
}

void non_set() {
}

void kernelSweep() {
    static uint32_t last = 0;
    if (millis() - last < 5000) return;
    last = millis();

    MEM::checkHeapTrend();
}

void ui_watchdog(void* arg) {
    vTaskDelay(pdMS_TO_TICKS(1000));

    while (1) {
        if (millis() - lastTick_wd > 20000) {
            panic(PANIC_UNKNOWN, "Unknown Panic Control");
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void enable_pm() {
    esp_pm_config_esp32c3_t pm = {
        .max_freq_mhz = 160,
        .min_freq_mhz = 40,
        .light_sleep_enable = false};

    esp_pm_configure(&pm);
}

void showFinalErrorScreen();
void showSuccessScreen();

unsigned long lastActive = 0;
static unsigned long lastTick = 0;
bool isOnSleepMode = false;
volatile bool pendingLockscreen = false;

const char* menuItems[] = {
    "WiFi Scan", "WiFi Connect", "Settings", "Brightness", "Browser",
    "WiFi Sniffer", "Beacon Spammer", "Pong", "Hotspot", "Ruler",
    "Keyboard Cat", "WiFi Storm", "Echo Sniffer", "IR Blaster", "Flappy Bird",
    "Factory Reset", "Check System Integrity", "Online Story", "Heap Monitor",
    "Stopwatch", "BenchMyESP", "Minecraft", "Stardew Valley", "BLE Scan", "C3 Burner",
    "WiFi Telnet", "Doom GL", "File Manager", "Store Manager", "Timer"};

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

void initMenuButton();
void IRAM_ATTR onButton() {
    lastActive = millis();
}

void showActionGUI() {
    display.clearBuffer();
    display.setBitmapMode(1);

    display.drawRFrame(31, 14, 67, 36, 5);
    display.drawXBM(34, 17, 19, 20, image_power_bits);
    display.drawXBM(55, 17, 19, 20, image_play_bits);
    display.drawXBM(76, 17, 19, 20, image_off_bits);

    display.drawXBM(36, 39, 10, 8, image_Pin_back_arrow_bits);

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(49, 46, "Back");
    display.sendBuffer();

    btnAction.attachClick([]() {
        shutdown_now();
    });

    btnAction.attachDoubleClick([]() {
        restart_now();
    });

    btnAction.attachMultiClick([]() {
        boot_mode = BOOT_NORMAL;

        display.clearBuffer();

        draw_restartESP8266();
        setupESP8266Communication();
        sendCommand("32:start");
        delay(100);
        sendCommand("avr32:force-restart");
        delay(100);

        display.clearBuffer();
        display.drawXBM(48, 12, 32, 40, image_Updating_bits);
        display.sendBuffer();

        setupESP8266Communication();
        sendCommand("32:start");
        delay(100);

        sendCommand("avr32:test");
        awaitResponse("ESP8266:AVR32");
        drawMenu();
    });

    btnAction.attachLongPressStart([]() {
        drawMenu();
    });
}

void drawMenuItemsWithOffset(float scrollOffset) {
    SCROLL_DURATION = sysConfig.scroll_duration;
    const int textOffsetY = sysConfig.text_offset_y;
    const int drawCount = sysConfig.draw_count;

    int intOffset = (int)floor(scrollOffset);
    float fractionalOffset = scrollOffset - (float)intOffset;

    float targetBoxY = (menuIndex - scrollOffset) * ITEM_HEIGHT;

    if (!isScrolling) {
        currentBoxY = targetBoxY;
    } else {
        currentBoxY += (targetBoxY - currentBoxY) * 0.35f;
    }

    display.setDrawColor(1);
    display.drawRBox(0, (int)currentBoxY, 128, ITEM_HEIGHT, 3);

    for (int i = 0; i < drawCount; i++) {
        int itemIndex = i + intOffset;
        if (itemIndex < 0 || itemIndex >= menuCount) continue;

        int yPos = (int)((i * ITEM_HEIGHT) - (fractionalOffset * ITEM_HEIGHT));

        if (yPos < -ITEM_HEIGHT || yPos > 64) continue;

        if (itemIndex == menuIndex) {
            display.setDrawColor(0);
        } else {
            display.setDrawColor(1);
        }

        display.drawStr(8, yPos + textOffsetY, menuItems[itemIndex]);
    }
}

void drawMenu() {
    setCpuFrequencyMhz(160);

    appRunning = false;
    currentApp = -1;
    anrActive = sysConfig.anr_active;
    systemUIActive = sysConfig.system_ui_active;

    display.setFont(u8g2_font_7x14_tr);
    display.clearBuffer();

    initMenuButton();

    float currentOffset;
    if (isScrolling) {
        unsigned long currentTime = millis();
        float progress = (currentTime - scrollStartTime) / (float)SCROLL_DURATION;

        if (progress >= 1.0f) {
            menuScrollOffset = (float)targetScrollOffset;
            isScrolling = false;
            currentOffset = menuScrollOffset;
        } else {
            float easedProgress = easeOutCubic(progress);
            currentOffset = menuScrollOffset + ((float)targetScrollOffset - menuScrollOffset) * easedProgress;
        }
    } else {
        currentOffset = (float)targetScrollOffset;
        menuScrollOffset = (float)targetScrollOffset;
    }

    drawMenuItemsWithOffset(currentOffset);

    display.setDrawColor(1);
    display.sendBuffer();
}

void updateScroll() {
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
            menuScrollOffset = menuScrollOffset + ((float)targetScrollOffset - menuScrollOffset) * easeOutCubic(progress);
        } else {
            menuScrollOffset = (float)targetScrollOffset;
        }

        targetScrollOffset = newScrollOffset;
        isScrolling = true;
        scrollStartTime = millis();
    }
}

void runApp(int index) {
    if (index < 0 || index >= menuCount) return;

    AppDesc& app = appTable[index];
    if (ESP.getFreeHeap() < app.minHeap) return;

    int startW = 20;
    int startH = 10;
    int startX = 64 - (startW / 2);
    int startY = 32 - (startH / 2);

    for (int i = 0; i <= 10; i++) {
        float t = i / 10.0f;
        float e = easeOutCubic(t);

        int curW = startW + (int)((128 - startW) * e);
        int curH = startH + (int)((64 - startH) * e);
        int curX = 64 - (curW / 2);
        int curY = 32 - (curH / 2);

        display.setDrawColor(1);
        display.drawRBox(curX, curY, curW, curH, 4 - (int)(4 * e));

        if (i < 7) {
            display.setDrawColor(0);
            display.setFont(u8g2_font_5x8_tr);

            int tw = display.getStrWidth(app.name);
            display.drawStr(64 - (tw / 2), 32 + 3, app.name);
        }

        display.sendBuffer();
        delay(15);
    }

    currentApp = index;
    appRunning = true;
    appLastBeat = millis();

    applyCapabilityBoundary(app.caps);
    app.entry();

    appRunning = false;
    currentApp = -1;

    display.clearBuffer();
    display.sendBuffer();

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
        showActionGUI();
    });

    if (anrActive) return;
}

void setup() {
    Serial.begin(115200);

    delay(500);
    Wire.begin(SDA_PIN, SCL_PIN, 900000);
    display.setBusClock(900000);
    delay(50);

    display.begin();
    display.setContrast(200);
    display.setFont(u8g2_font_6x10_tr);
    display.setBitmapMode(1);

    display.sendBuffer();

    lastTick_wd = millis();

    boot();
}

void loop() {
    btnUp.tick();
    btnDown.tick();
    btnOK.tick();
    btnAction.tick();

    timerTick();

    gPower.loop();

    if (pendingLockscreen) {
        pendingLockscreen = false;
        showLockscreen(true);
    }

    if (isScrolling) drawMenu();

    if (millis() - lastActive > SLEEP_TIMEOUT) {
        enterSleep();
        lastActive = millis();
        isOnSleepMode = true;
    }

    if (WiFi.getMode() == WIFI_AP || WiFi.softAPgetStationNum() > 0) {
        dnsServer.processNextRequest();
    }

    server.handleClient();
    processSerialInput();

    static uint32_t last;
    static uint32_t last_heap;

    if (millis() - last > 5000) {
        last = millis();
        Serial.printf("Stack left: %u words\n",
                      uxTaskGetStackHighWaterMark(NULL));
    }

    if (millis() - last_heap > 5000) {
        last_heap = millis();
        Serial.printf("Free heap: %u\n", ESP.getFreeHeap());
    }

    MEM::monitorLoop("MainLoop");
    MEM::checkHeapTrend();

    kernelSweep();

    lastTick_wd = millis();

    if (ESP.getFreeHeap() < 2 * 1024) {
        panic(PANIC_MEM_CORRUPT, "Heap allocation failed");
        return;
    }

    anr_update_handler();

    vTaskDelay(1);
}