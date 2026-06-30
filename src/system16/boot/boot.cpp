#include "boot.h"

#include <string>
#include <vector>

#include "component/hardware/eeprom.h"
#include "component/rp2040link.h"
#include "component/queuer.h"
#include "component/ui_enhancements.h"
#include "component/wifiservice.h"
#include "esp_task_wdt.h"
#include "system16/AT24C256_EEPROM.h"
#include "system16/CrashLog.h"
#include "system16/loader.h"
#include "system16/oled_animation.h"
#include "system16/state.h"
#include "system16/update.h"
#include "app/Essential/Settings.h"

std::vector<String> bootLogs;
const int MAX_LOG_LINES = 5;

void addBootLog(String msg) {
    bootLogs.push_back("> " + msg);
    if (bootLogs.size() > MAX_LOG_LINES) bootLogs.erase(bootLogs.begin());
}

// Start of Safe boot logging
static const uint8_t SB_MAX_LINES = 32;
static const uint8_t SB_LINE_H = 7;   // px per line (font 4x6 = 6px + 1 gap)
static const uint8_t SB_VISIBLE = 6;  // Adjusted for logo and progress bar
static const uint8_t SB_LINE_W = 31;  // max chars per line (128px / 4px font)

static char sb_buf[SB_MAX_LINES][SB_LINE_W + 1];
static uint8_t sb_count = 0;
static uint8_t sb_scroll = 0;  // first visible line index
static float bootProgress = 0.0f;

static void drawBootUI() {
    display.clearBuffer();
    display.setDrawColor(1);

    // Breathing centered C3OS logo
    float breath = (sin(millis() / 600.0) + 1.0) / 2.0;
    int logoSize = 22 + (int)(breath * 4);
    int logoX = 64 - logoSize / 2;
    int logoY = 22 - logoSize / 2;
    display.drawXBM(logoX, logoY, logoSize, logoSize, image_C3_bits);

    // Rotating circular progress ring
    float angle = (millis() / 800.0) * 6.2832;
    float ringRadius = 18.0f;
    int cx = 64, cy = 22;
    int arcPoints = (int)(bootProgress * 20);
    for (int i = 0; i < arcPoints; i++) {
        float a = angle + (i / 20.0f) * 6.2832f;
        int px = cx + (int)(cos(a) * ringRadius);
        int py = cy + (int)(sin(a) * ringRadius);
        display.drawPixel(px, py);
    }

    // Clean thin progress bar at bottom
    display.drawFrame(4, 56, 120, 6);
    int barWidth = (int)(bootProgress * 118);
    if (barWidth > 0) {
        display.drawBox(5, 57, barWidth, 4);
    }

    display.sendBuffer();
}

static void sb_push(const char* text) {
    // wrap into buffer
    uint8_t slot = sb_count % SB_MAX_LINES;
    strncpy(sb_buf[slot], text, SB_LINE_W);
    sb_buf[slot][SB_LINE_W] = '\0';
    sb_count++;

    // auto-scroll: keep newest lines visible
    if (sb_count > SB_VISIBLE) sb_scroll = sb_count - SB_VISIBLE;

    drawBootUI();
    Serial.printf("[BOOT] %s\n", text);
}

static void updateBootProgress(float p) {
    bootProgress = p;
    drawBootUI();
}

#define SB_BEGIN(label)                                       \
    do {                                                      \
        char _sb_pre[SB_LINE_W + 1];                          \
        snprintf(_sb_pre, sizeof(_sb_pre), "> %s...", label); \
        sb_push(_sb_pre);                                     \
        esp_task_wdt_reset();                                 \
    } while (0);                                              \
    uint32_t _sb_t0 = millis();                               \
    const char* _sb_label = label;

#define SB_END(status)                                                   \
    do {                                                                 \
        char _sb_post[SB_LINE_W + 1];                                    \
        snprintf(_sb_post, sizeof(_sb_post), "< %s %lums %s", _sb_label, \
                 (unsigned long)(millis() - _sb_t0), status);            \
        sb_push(_sb_post);                                               \
        esp_task_wdt_reset();                                            \
    } while (0)

#define SB_NOTE(fmt, ...)                                              \
    do {                                                               \
        char _sb_note[SB_LINE_W + 1];                                  \
        snprintf(_sb_note, sizeof(_sb_note), "  " fmt, ##__VA_ARGS__); \
        sb_push(_sb_note);                                             \
    } while (0)
// End of Safe boot logging

void scanI2CBus();
void showSuccessScreen();
void showFinalErrorScreen();
bool eepromAvailable();

uint8_t readEEPROM(uint16_t addr);
void writeEEPROM(uint16_t addr, uint8_t val);

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

// Easing functions
static float easeOutElastic(float t) {
    if (t <= 0) return 0;
    if (t >= 1) return 1;
    const float c4 = (2.0f * PI) / 3.0f;
    return powf(2, -10 * t) * sinf((t * 10 - 0.75f) * c4) + 1;
}
static float easeInQuart(float t) { return t * t * t * t; }
static float smoothstep(float t) { return t * t * (3 - 2 * t); }
static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// ── draw helpers (all relative to display coords 128×64) ──────
static void drawSquare(U8G2& d, int cx, int cy, int half) {
    d.drawFrame(cx - half, cy - half, half * 2, half * 2);
}
static void drawTriangle(U8G2& d, int cx, int cy, int r) {
    d.drawLine(cx, cy - r, cx - r, cy + r);
    d.drawLine(cx - r, cy + r, cx + r, cy + r);
    d.drawLine(cx + r, cy + r, cx, cy - r);
}

RTC_DATA_ATTR int boot_count = 0;

extern uint32_t SLEEP_TIMEOUT;
extern void enable_pm();
extern uint8_t getBatteryPercent();

uint16_t cpuFrequency = 160;
bool isFastBoot = false;

// EEPROM Recovery and Checking
void initEEPROM_Failed() {
    const unsigned long RECONNECT_TIMEOUT = 10000;
    const unsigned long RETRY_INTERVAL = 500;
    unsigned long startTime = millis();
    int retryCount = 0;

    for (int i = 0; i < 3; i++) {
        display.clearBuffer();
        display.drawXBM(5, 11, 35, 43, image_SDQuestion_bits);

        display.setFont(u8g2_font_helvB10_tr);
        display.drawStr(45, 20, "EEPROM ERROR");

        display.setFont(u8g2_font_5x8_tr);
        display.drawStr(45, 32, "I2C Device Missing");
        display.drawStr(45, 42, "Expected: 0x50");
        display.drawStr(45, 52, "Scanning for devices...");

        display.sendBuffer();
        delay(300);

        if (eepromAvailable()) return;
    }

    scanI2CBus();

    while (millis() - startTime < RECONNECT_TIMEOUT) {
        retryCount++;

        display.clearBuffer();

        static uint8_t animFrame = 0;
        const uint8_t* animIcons[] = {image_Restoring_bits, image_SDQuestion_bits};
        display.drawXBM(5, 11, 35, 43, animIcons[animFrame % 2]);
        animFrame++;

        display.setFont(u8g2_font_helvB08_tr);
        display.drawStr(45, 16, "EEPROM RECOVERY");

        display.setFont(u8g2_font_5x8_tr);
        display.drawStr(45, 28, "Attempting reconnect...");

        char statusBuf[32];
        snprintf(statusBuf, sizeof(statusBuf), "Retry: %d/%d", retryCount,
                 RECONNECT_TIMEOUT / RETRY_INTERVAL);
        display.drawStr(45, 38, statusBuf);

        unsigned long elapsed = millis() - startTime;
        unsigned long remaining = RECONNECT_TIMEOUT - elapsed;
        snprintf(statusBuf, sizeof(statusBuf), "Time left: %lu.%lus", remaining / 1000,
                 (remaining % 1000) / 100);
        display.drawStr(45, 48, statusBuf);

        int progressWidth = map(elapsed, 0, RECONNECT_TIMEOUT, 0, 81);
        display.drawFrame(45, 55, 81, 6);
        display.drawBox(45, 55, progressWidth, 6);

        uint8_t dotPos = (millis() / 200) % 4;
        display.drawBox(120, 28 + (dotPos * 2), 2, 2);

        display.sendBuffer();

        if (eepromAvailable()) {
            showSuccessScreen();
            delay(800);
            return;
        }

        delay(RETRY_INTERVAL);
    }

    showFinalErrorScreen();
}

void showSuccessScreen() {
    for (int i = 0; i < 2; i++) {
        display.clearBuffer();
        display.drawXBM(45, 16, 38, 32, image_Restoring_bits);

        display.setFont(u8g2_font_helvB10_tr);
        display.drawStr(0, 55, "EEPROM RESTORED!");

        if (i == 1) {
            display.setFont(u8g2_font_unifont_t_symbols);
            display.drawGlyph(100, 55, 0x2713);
        }

        display.sendBuffer();
        delay(400);
    }
    delay(600);
}

void showFinalErrorScreen() {
    bool showAlternate = false;
    unsigned long lastToggle = 0;

    while (true) {
        unsigned long currentTime = millis();

        display.clearBuffer();
        display.drawXBM(5, 11, 35, 43, image_SDQuestion_bits);

        display.setFont(u8g2_font_helvB08_tr);
        display.drawStr(45, 16, "HARDWARE ERR");

        display.setFont(u8g2_font_4x6_tr);
        display.drawStr(45, 26, "EEPROM Not Found");
        display.drawStr(45, 34, "Addr: 0x50 (NACK)");
        display.drawStr(45, 42, "Check Connections");
        display.drawStr(45, 50, "I2C Pull-up Resistors");

        if ((currentTime / 650) % 2 == 0) {
            display.setFont(u8g2_font_4x6_tr);
            display.drawStr(0, 63, ">>> PRESS OK <<<");
        }

        display.sendBuffer();

        btnOK.attachClick([]() {
            display.clearBuffer();
            display.sendBuffer();

            esp_restart();
        });

        btnOK.tick();
        delay(50);
    }
}

void initEEPROM() {
    uint8_t flag = readEEPROM(INIT_FLAG_ADDR);

    if (flag == 0xAA) {
        Serial.println("EEPROM Already Initialized!");
        return;
    }

    uint8_t page[PAGE_SIZE];
    for (int i = 0; i < PAGE_SIZE; i++) page[i] = 0xFF;

    for (uint16_t addr = 1; addr < EEPROM_SIZE; addr += PAGE_SIZE) {
        for (uint8_t i = 0; i < PAGE_SIZE; i += 32) {
            Wire.beginTransmission(EEPROM_ADDR);
            Wire.write(((addr + i) >> 8) & 0xFF);
            Wire.write((addr + i) & 0xFF);
            Wire.write(&page[i], 32);
            Wire.endTransmission();
            delay(5);
        }
        esp_task_wdt_reset();  // Reset watchdog after each page write
    }

    writeEEPROM(INIT_FLAG_ADDR, 0xAA);
    Serial.println("EEPROM initialized");
}

void writeEEPROM(uint16_t addr, uint8_t val) {
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        Wire.beginTransmission(EEPROM_ADDR);
        Wire.write((addr >> 8) & 0xFF);
        Wire.write(addr & 0xFF);
        Wire.write(val);
        Wire.endTransmission();
        xSemaphoreGive(i2cMutex);
        delay(5);
    }
}

uint8_t readEEPROM(uint16_t addr) {
    uint8_t val = 0xFF;
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        Wire.beginTransmission(EEPROM_ADDR);
        Wire.write((addr >> 8) & 0xFF);
        Wire.write(addr & 0xFF);
        Wire.endTransmission();

        Wire.requestFrom(EEPROM_ADDR, 1);
        if (Wire.available()) val = Wire.read();
        xSemaphoreGive(i2cMutex);
    }
    return val;
}

bool eepromAvailable() {
    bool available = false;
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        Wire.beginTransmission(0x50);
        uint8_t err = Wire.endTransmission();
        available = (err == 0);
        xSemaphoreGive(i2cMutex);
    }
    return available;
}

void scanI2CBus() {
    display.clearBuffer();
    display.setFont(u8g2_font_helvB08_tr);
    display.drawStr(0, 12, "Scanning I2C Bus...");

    byte foundCount = 0;
    for (byte addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        byte error = Wire.endTransmission();

        display.drawBox(0, 20, (addr * 100) / 127, 4);
        display.sendBuffer();

        if (error == 0) {
            foundCount++;
            char buf[20];
            snprintf(buf, sizeof(buf), "Found: 0x%02X", addr);
            display.setFont(u8g2_font_5x8_tr);
            display.drawStr(0, 35 + (foundCount * 8), buf);
            display.sendBuffer();
        }
        delay(2);
    }
    delay(800);
}

// Sleep
void enterSleep() {
    esp_task_wdt_delete(NULL);
    esp_task_wdt_reset();

    animateOLEDOff();
    esp_task_wdt_reset();

    isOnSleepMode = true;

    stopAllService();
    esp_task_wdt_reset();

    WiFi.mode(WIFI_OFF);

    gpio_deep_sleep_hold_dis();

    gpio_hold_en((gpio_num_t)BUTTON_OK);
    gpio_wakeup_enable((gpio_num_t)BUTTON_OK, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();

    esp_light_sleep_start();

    gpio_hold_dis((gpio_num_t)BUTTON_OK);

    esp_task_wdt_add(NULL);

    esp_task_wdt_reset();
    startService();
    esp_task_wdt_reset();

    isOnSleepMode = false;
    fadeInOLED();
    esp_task_wdt_reset();

    pendingLockscreen = true;
    WiFi.mode(WIFI_AP_STA);
    esp_task_wdt_reset();

    drawMenu();
}

// Boot
void log(String msg) {
    Serial.print("[");
    Serial.print("  " + String(millis()));
    Serial.print("] ");

    Serial.println(msg);
    Serial.flush();
}

const char* logBuffer[MAX_LINES];
int logCount = 0;
float scrollY_Log = 0;
bool isScrolling_Log = false;

void printLog(const char* text) {
    if (logCount < MAX_LINES) {
        logBuffer[logCount++] = text;
    } else {
        for (int i = 1; i < MAX_LINES; i++) {
            logBuffer[i - 1] = logBuffer[i];
        }

        logBuffer[MAX_LINES - 1] = text;
    }

    scrollY_Log += LINE_HEIGHT;
    isScrolling_Log = true;
}

void drawLog() {
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);

    if (isScrolling_Log) {
        scrollY_Log *= 0.75f;
        if (scrollY_Log < 0.5f) {
            scrollY_Log = 0;
            isScrolling_Log = false;
        }
    }

    int maxVisible = SCREEN_H / LINE_HEIGHT;
    int startIndex = max(0, logCount - maxVisible);

    for (int i = 0; i < maxVisible; i++) {
        int idx = startIndex + i;
        if (idx >= logCount) break;

        int yPos = (i * LINE_HEIGHT) - (int)scrollY_Log + LINE_HEIGHT;

        if (yPos >= 0 && yPos <= SCREEN_H) {
            display.drawStr(0, yPos, logBuffer[idx]);
        }
    }

    display.sendBuffer();
}

void logDelay(int ms) {
    unsigned long startTime = millis();
    while (millis() - startTime < ms) {
        drawLog();
        delay(10);
    }
}

void fast_boot() {
    pinMode(BUTTON_ACTION, INPUT_PULLUP);
    esp_log_level_set("*", ESP_LOG_INFO);

    display.clearBuffer();
    display.setBitmapMode(1);

    display.drawXBM(58, 20, 11, 11, image_C3_bits);

    display.sendBuffer();

    fadeInOLED();

    static float visualP = 0.0;

    auto syncProgress = [&](float target) {
        while (visualP < target) {
            visualP += (target - visualP) * 0.15f + 0.005f;
            if (visualP > target) visualP = target;

            display.clearBuffer();

            display.setBitmapMode(1);
            display.drawXBM(58, 20, 11, 11, image_C3_bits);

            float timeSec = millis() / 1000.0;
            float breath = (sin(timeSec * 3.0) + 1.0) / 2.0;

            int lineWidth = 10 + (int)(breath * 30);

            display.drawHLine(64 - (lineWidth / 2), 52, lineWidth);

            display.drawPixel(64 - (lineWidth / 2) - 3, 52);
            display.drawPixel(64 + (lineWidth / 2) + 3, 52);

            display.sendBuffer();
            delay(15);

            if (target - visualP < 0.005) {
                visualP = target;
                break;
            }
        }
    };

    initBackgroundManager();

    esp_reset_reason_t cause = esp_reset_reason();
    if (cause == ESP_RST_BROWNOUT) {
        batteryDead();
    }

    AT24C256 eeprom(0x50);
    eeprom.begin();

    CrashLogger logger;
    logger.begin(eeprom);

    ResetType reset = logger.checkResetReason();

    if (reset == RESET_BROWNOUT) {
        logger.logEvent(RESET_BROWNOUT);

        display.clearBuffer();
        display.setFont(u8g2_font_6x10_tf);
        display.drawStr(25, 25, "Battery");
        display.drawStr(20, 40, "Depleted");
        display.setFont(u8g2_font_4x6_tr);
        display.drawStr(15, 55, "Charge to continue");
        display.sendBuffer();
        delay(2000);
    }

    Serial.println("[BOOT] Mounting LittleFS...");
    syncProgress(0.3);
    bool fsOk = LittleFS.begin(true);

    Serial.println("[BOOT] Init Services...");
    syncProgress(0.65);
    registerService("WiFiCheck", BA_WiFi, 4096);
    registerService("WiFiMonitor", WiFiMonitorService, 4096);
    registerService("Timer", BA_TIMERTICK, 2048);
    registerService("EME_RESTART", BA_EME_RESTART_COMBINATION, 2048);

    syncProgress(0.95);
    setting.loadSettings();
    SLEEP_TIMEOUT = Settings::instance->get().sleepTimeout;
    cpuFrequency = Settings::instance->get().cpuFrequency;
    setCpuFrequencyMhz(cpuFrequency);

    if (esp8266TaskHandle != NULL) vTaskSuspend(esp8266TaskHandle);
    setupESP8266Communication();
    sendCommand("32:start");
    if (esp8266TaskHandle != NULL) vTaskResume(esp8266TaskHandle);

    syncProgress(1.0);

    display.clearBuffer();
    display.sendBuffer();

    if (!LittleFS.exists("/settings.bin")) {
        display.setFont(u8g2_font_4x6_tr);
        display.drawStr(22, 35, "Preparing Auto Repair");
        display.sendBuffer();

        Settings::instance->loadSettings();

        if (!LittleFS.exists("/settings.bin")) {
            display.clearBuffer();
            display.setFont(u8g2_font_4x6_tr);
            display.drawStr(28, 31, "Auto Repair Failed");
            display.drawStr(26, 38, "/settings.bin : NIL");

            display.drawStr(0, 64, "[OK] Skip");
            display.sendBuffer();
        }

        while (true) {
            if (digitalRead(BUTTON_OK) == LOW) {
                display.clearBuffer();
                display.sendBuffer();

                break;
            }

            delay(320);
        }
    }

    display.sendBuffer();
    boot_count = 0;

    {
        bool hasSettings = (readEEPROM(SETTINGS_MARKER_ADDR) == SETTINGS_MARKER_VAL);
        display.setFont(u8g2_font_6x10_tr);
        display.clearBuffer();
        display.drawStr(0, 15, "EEPROM: 32KB (AT24C256)");
        display.drawStr(0, 30, hasSettings ? "Settings: SAVED" : "Settings: EMPTY");
        display.sendBuffer();
        delay(1000);
    }

    showLockscreen(true);
}

void safe_boot();
static bool gPowerSaveForced = false;

void full_boot() {
    pinMode(BUTTON_ACTION, INPUT_PULLUP);
    esp_log_level_set("*", ESP_LOG_INFO);

    boot_count++;
    if (boot_count > 3) {
        boot_count = 0;
        panic(PANIC_IPC_DROP, "RECOVERY MODE ACTIVE");
        runBIOS();
        return;
    }

    // Check for brownout or low battery
    {
        esp_reset_reason_t rst_reason = esp_reset_reason();
        if (rst_reason == ESP_RST_BROWNOUT || getBatteryPercent() < 15) {
            display.clearBuffer();
            display.setFont(u8g2_font_6x10_tf);
            if (rst_reason == ESP_RST_BROWNOUT) {
                display.drawStr(10, 20, "BROWNOUT DETECTED");
            } else {
                display.drawStr(15, 20, "LOW BATTERY");
            }
            display.setFont(u8g2_font_4x6_tr);
            display.drawStr(10, 35, "Enable Power Save Mode?");
            display.drawStr(10, 50, "OK=Enable  ACT=Skip");
            display.sendBuffer();

            unsigned long waitStart = millis();
            bool enablePowerSave = false;
            while (millis() - waitStart < 10000) {
                esp_task_wdt_reset();
                if (digitalRead(BUTTON_OK) == LOW) {
                    enablePowerSave = true;
                    break;
                }
                if (digitalRead(BUTTON_ACTION) == LOW) {
                    break;
                }
                delay(50);
            }
            if (enablePowerSave) {
                setCpuFrequencyMhz(80);
                gPower.setProfile(PowerManager::PowerSave);
                gPowerSaveForced = true;
                bootLogs.push_back("> PowerSave enabled");
            }
        }
    }

    display.clearBuffer();
    display.setContrast(255);
    updateBootProgress(0.02f);

    auto lnx_log = [&](const char* msg, float p = -1.0f) {
        char buf[64];
        snprintf(buf, sizeof(buf), "[%8.6f] %s", millis() / 1000.0, msg);
        if (p >= 0.0f) bootProgress = p;
        sb_push(buf);
        delay(10);
        esp_task_wdt_reset();
    };

    lnx_log("Linux version 5.15.0-C3OS", 0.05f);
    lnx_log("CPU: ESP32-C3 rev 3 (160MHz)", 0.07f);
    lnx_log("Memory: 400KB available", 0.09f);
    lnx_log("arch_sys_counter: 16000 kHz", 0.11f);
    lnx_log("random: crng init done", 0.13f);
    lnx_log("rtc: system clock sync'd", 0.15f);
    
    if (digitalRead(BUTTON_ACTION) == LOW) {
        lnx_log("Interrupt: action_btn triggered");
        runBIOS();
        return;
    }

    lnx_log("Probing partition table...", 0.18f);
    lnx_log("  nvs: 20KB @ 0x9000", 0.20f);
    lnx_log("  otadata: 8KB @ 0xe000", 0.22f);
    lnx_log("  app0: 1280KB @ 0x10000", 0.24f);
    lnx_log("  spiffs: 1472KB @ 0x260000", 0.26f);

    lnx_log("Checking I2C bus...", 0.28f);
    Wire.beginTransmission(0x50);
    if (Wire.endTransmission() == 0) {
        lnx_log("i2c: found device at 0x50 (EEPROM)", 0.30f);
        initEEPROM();
        lnx_log("eeprom: AT24C256 identified", 0.32f);
        lnx_log("eeprom: AT24C256 32KB initialized", 0.34f);
    } else {
        lnx_log("i2c: no eeprom found at 0x50", 0.32f);
    }

    if (RP2040Link::detect()) {
        lnx_log("rp2040: coprocessor found on I2C", 0.35f);
    } else {
        lnx_log("rp2040: not detected", 0.35f);
    }

    lnx_log("input: OneButton v2.0.0 driver", 0.35f);
    lnx_log("Mounting LittleFS...", 0.40f);
    if (LittleFS.begin(true)) {
        lnx_log("vfs: mounted littlefs on /", 0.45f);
    } else {
        lnx_log("vfs: mount failed!", 0.45f);
    }

    lnx_log("Loading configuration...", 0.50f);
    loadConfig();
    lnx_log("config: system params loaded", 0.52f);

    lnx_log("Initializing background manager...", 0.55f);
    initBackgroundManager();

    lnx_log("PowerManager: starting...", 0.60f);
    enable_pm();
    gPower.begin(PowerManager::Balanced);
    lnx_log("pm: balanced profile active", 0.65f);

    if (sysConfig.init_service) {
        lnx_log("svc: registering kernel tasks...", 0.70f);
        registerService("CPUTemp", BA_CPUTemp, 2048);
        registerService("WiFiCheck", BA_WiFi, 4096);
        registerService("WiFiMonitor", WiFiMonitorService, 4096);
        registerService("Timer", BA_TIMERTICK, 2048);
        registerService("Pocket Detection", BA_POCKET_DETECTION, 2048);
        registerService("ESP8266 Check", BA_ESP8266_CHECK, 2048);
        lnx_log("svc: all services spawned", 0.75f);
    }

    lnx_log("Applying system settings...", 0.80f);
    setting.loadSettings();
    esp_task_wdt_reset();  // Reset watchdog after loading settings
    SLEEP_TIMEOUT = Settings::instance->get().sleepTimeout;
    cpuFrequency = Settings::instance->get().cpuFrequency;
    setCpuFrequencyMhz(cpuFrequency);
    esp_task_wdt_reset();  // Reset watchdog after setting CPU frequency

    lnx_log("Probing coprocessor (ESP8266)...", 0.85f);
    if (!sysConfig.no_init) {
        setupESP8266Communication();
        esp_task_wdt_reset();  // Reset watchdog after ESP8266 setup
        sendCommand("32:start");
        esp_task_wdt_reset();  // Reset watchdog after sending command
        lnx_log("uart: bridge established", 0.90f);
    }

    lnx_log("Starting networking stack...", 0.92f);
    lnx_log("net: radio initialized", 0.94f);
    esp_task_wdt_reset();  // Reset watchdog after networking init

    lnx_log("Running system startup...", 0.96f);
    applyPendingUpdate();
    esp_task_wdt_reset();  // Reset watchdog after update check
    startService();
    esp_task_wdt_reset();  // Reset watchdog after starting services

    lnx_log("C3OS-init: boot finished", 1.0f);
    esp_task_wdt_reset();  // Reset watchdog before final delay
    delay(500);

    {
        bool hasSettings = (readEEPROM(SETTINGS_MARKER_ADDR) == SETTINGS_MARKER_VAL);
        display.clearBuffer();
        display.setFont(u8g2_font_6x10_tr);
        display.drawStr(0, 15, "EEPROM: 32KB (AT24C256)");
        display.drawStr(0, 30, hasSettings ? "Settings: SAVED" : "Settings: EMPTY");
        display.drawStr(0, 50, "Hold ACTION for BIOS");
        display.sendBuffer();
        delay(1000);
    }

    boot_count = 0;
    UX::TransitionEffects::fadeOut();
    showLockscreen(true);
}

void safe_boot() {
    pinMode(BUTTON_ACTION, INPUT_PULLUP);
    esp_log_level_set("*", ESP_LOG_ERROR);

    // ── Display init ─────────────────────────────────────────
    {
        SB_BEGIN("Display");
        fadeInOLED();
        SB_END("OK");
    }

    // ── Chip info ────────────────────────────────────────────
    {
        SB_BEGIN("Chip info");
        uint8_t rev = ESP.getChipRevision();
        uint32_t freq = ESP.getCpuFreqMHz();
        uint32_t flash = ESP.getFlashChipSize() / 1024;
        SB_END("OK");
        SB_NOTE("Rev=%d Freq=%luMHz", rev, (unsigned long)freq);
        SB_NOTE("Flash=%luKB", (unsigned long)flash);
        SB_NOTE("Free=%luB", (unsigned long)ESP.getFreeHeap());
    }

    // ── Wake / BIOS check ────────────────────────────────────
    {
        SB_BEGIN("Wake source");
        esp_reset_reason_t reason = esp_reset_reason();
        SB_END("OK");
        SB_NOTE("Reset cause=%d", (int)reason);

        if (digitalRead(BUTTON_ACTION) == LOW) {
            sb_push("! ACTION held — BIOS");
            delay(200);
            runBIOS();
            return;
        }
    }

    // ── Memory fence (RISC-V safety) ────────────────────────
    {
        SB_BEGIN("Memory fence");
        asm volatile("fence");
        asm volatile("fence iorw, iorw");
        SB_END("OK");
    }

    // ── NOPs (pipeline flush) ────────────────────────────────
    {
        SB_BEGIN("Pipeline flush");
        for (int i = 0; i < 1000; i++) asm volatile("nop");
        SB_END("OK");
    }

    // ── CSR snapshot ────────────────────────────────────────
    {
        SB_BEGIN("CSR init");
        asm volatile("csrr t0, mepc \n csrw mscratch, t0 \n");
        SB_END("OK");
    }

    // ── WiFi off ─────────────────────────────────────────────
    {
        SB_BEGIN("WiFi stack");
        WiFi.mode(WIFI_OFF);
        btStop();
        SB_END("OFF");
        SB_NOTE("BT stopped");
    }

    // ── LittleFS ─────────────────────────────────────────────
    {
        SB_BEGIN("LittleFS");
        bool ok = LittleFS.begin(true);
        SB_END(ok ? "OK" : "FAIL");
        if (ok) {
            SB_NOTE("Total=%zuKB Used=%zuKB", LittleFS.totalBytes() / 1024,
                    LittleFS.usedBytes() / 1024);
            loadConfig();
        } else {
            sb_push("! FS mount failed");
        }
    }

    // ── Settings ─────────────────────────────────────────────
    {
        SB_BEGIN("Settings");
        setting.loadSettings();
        SLEEP_TIMEOUT = Settings::instance->get().sleepTimeout;
        uint16_t cpuMhz = Settings::instance->get().cpuFrequency;
        SB_END("OK");
        SB_NOTE("Sleep=%lus CPU=%dMHz", (unsigned long)(SLEEP_TIMEOUT / 1000), cpuMhz);
    }

    // ── CPU frequency ────────────────────────────────────────
    {
        uint16_t cpuMhz = gPowerSaveForced ? 80 : Settings::instance->get().cpuFrequency;
        if (cpuMhz < 80 || cpuMhz > 160) cpuMhz = 80;
        SB_BEGIN("CPU freq");
        setCpuFrequencyMhz(cpuMhz);
        SB_END("OK");
        SB_NOTE("Set to %dMHz", cpuMhz);
    }

    // ── Power manager ────────────────────────────────────────
    {
        SB_BEGIN("PowerManager");
        enable_pm();
        PowerManager::Profile pmProfile = gPowerSaveForced
                                              ? PowerManager::PowerSave
                                              : (Settings::instance->get().gameMode
                                                     ? PowerManager::Performance
                                                     : PowerManager::Balanced);
        gPower.begin(pmProfile);
        gPower.setAutoSleepTimeout(Settings::instance->get().sleepTimeout);
        gPower.setPreSleepCallback([]() { stopAllService(); });
        gPower.setPostWakeCallback([]() { startService(); });
        gPower.setDisplayPowerCallback([](bool on) {
            if (on)
                fadeInOLED();
            else
                fadeOutOLED();
        });
        gPower.setDisplayDimCallback([](bool dim) {
            if (dim) display.setContrast(20);
            else display.setContrast(Settings::instance->get().oledContrast);
        });
        SB_END("OK");
        SB_NOTE("Mode=%s Sleep=%lums",
                pmProfile == PowerManager::Performance ? "Performance" :
                pmProfile == PowerManager::PowerSave ? "PowerSave" : "Balanced",
                (unsigned long)gPower.getAutoSleepTimeout());
    }

    // ── Background services ──────────────────────────────────
    {
        SB_BEGIN("Svc HeapAllocator");
        registerService("Heap Allocator", BA_HEAP_ALLOCATOR, 1024);
        SB_END("OK");
    }
    {
        SB_BEGIN("Svc EME_RESTART");
        registerService("EME_RESTART", BA_EME_RESTART_COMBINATION, 2048);
        SB_END("OK");
    }

    // ── ESP8266 coprocessor ──────────────────────────────────
    {
        SB_BEGIN("ESP8266 UART");
        setupESP8266Communication();
        SB_END("OK");
    }
    {
        SB_BEGIN("ESP8266 start cmd");
        sendCommand("32:start");
        SB_END("sent");
    }

    // ── RP2040 coprocessor ──────────────────────────────────
    {
        SB_BEGIN("RP2040 UART");
        bool found = RP2040Link::detect();
        SB_END(found ? "OK" : "N/A");
        if (found) SB_NOTE("Found on UART");
    }

    // ── Start all services ───────────────────────────────────
    {
        SB_BEGIN("startService");
        startService();
        SB_END("OK");
    }

    // ── Final heap report ────────────────────────────────────
    SB_NOTE("Heap free=%luB", (unsigned long)ESP.getFreeHeap());
    sb_push("-- safe boot done --");

    delay(600);  // let user read the last lines

    drawMenu();
}

void boot() {
    if (LittleFS.begin(true)) {
        isFastBoot = Settings::instance->get().fastboot;
    }

    if (isFastBoot) {
        fast_boot();
        return;
    } else {
        if (boot_mode == BOOT_SAFE) {
            boot_mode = BOOT_NORMAL;
            safe_boot();
        } else if (boot_mode == BOOT_NORMAL) {
            full_boot();
        }
    }
}