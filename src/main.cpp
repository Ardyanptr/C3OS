#include <Arduino.h>

#include "include/BLEAppLib.h"
#include "include/ComponentLib.h"
#include "include/CoreLib.h"
#include "include/EssentialAppLib.h"
#include "include/SystemCoreLib.h"
#include "include/UILib.h"

extern DNSServer dnsServer;
extern WebServer server;

#define SDA_PIN 4
#define SCL_PIN 5

#define SCREEN_H 64
#define LINE_H 16
#define MAX_LINES 16

#define BUTTON_UP 0
#define BUTTON_DOWN 1
#define BUTTON_OK 2
#define BUTTON_ACTION 3

#define EEPROM_ADDR 0x50
#define EEPROM_SIZE 32668
#define PAGE_SIZE 64
#define INIT_FLAG_ADDR 0

void drawMenu();

RTC_DATA_ATTR boot_mode_t boot_mode = BOOT_NORMAL;

String BOARD_ATTACHED = "ESP32-C3";
int BOARD_REV = 4;

U8G2_SSD1315_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

OneButton btnUp(BUTTON_UP, true);
OneButton btnDown(BUTTON_DOWN, true);
OneButton btnOK(BUTTON_OK, true);
OneButton btnAction(BUTTON_ACTION, true);

AT24C256 eep(0x50);

Settings setting;
PowerManager gPower;
DFRobot_VL53L0X sensor;

void showLockscreen(bool isWake);

bool isOnErrorFlag = true;
volatile uint32_t lastTick_wd;

int currentApp = -1;
bool appRunning = false;
uint32_t appLastBeat = 0;

bool anrActive = false;
int anrApp = -1;
uint8_t anrSelect = 0;

bool systemUIActive = true;

void killCurrentApp() {
    stopAllService();

    appRunning = false;
    currentApp = -1;
    anrActive = false;

    systemUIActive = true;
    drawMenu();
}

void appHeartBeat() {
    appLastBeat = millis();
}

void triggerPanic() {
    ((void (*)())0x00000000)();
}

void runSettings() {
    setting.begin();
    setting.run();
}  // bound

void factory_reset_run() {
    // factory_reset();
}  // bound

void check_sys_integrity_run() {
    check_sys_integrity();
}  // bound

void non_set() {
    // do nothing
}  // bound

void runGameClient_SWT() {
    GameClient client(BUTTON_UP, BUTTON_DOWN, BUTTON_ACTION, BUTTON_OK);

    client.begin();
    client.runGame();
}  // bound

void runGameClient_SVC() {
    GameClient_svc client(BUTTON_UP, BUTTON_DOWN, BUTTON_ACTION, BUTTON_OK);

    client.begin();
    client.runGame();
}  // bound

void runGameClient_DOOM() {
    GameClient_Doom client(BUTTON_UP, BUTTON_DOWN, BUTTON_OK, BUTTON_ACTION);

    client.begin();
    client.runGame();
}  // bound

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

String crashID() {
    unsigned long t = millis();
    size_t heap = ESP.getFreeHeap();
    int h3 = heap % 1000;
    uint8_t code = (uint8_t)esp_reset_reason();

    return "0x" + String(code, HEX) + String(h3, HEX) + String(t, HEX);
}

String createDump(String pCap, String mill, String cap, String pca) {
    String dump = pCap + "\n" + mill + "\n" + cap + "\n" + pca + "\n";
    return dump;
}

bool eepWriteTest() {
    Wire.beginTransmission(0x50);
    Wire.write(0x00);
    Wire.write(0x10);
    Wire.write(0xAB);
    return Wire.endTransmission() == 0;
}

bool dumpToEEPROM(AT24C256& e, uint16_t addr, const String& dump) {
    uint16_t len = dump.length();
    uint8_t buf[len];

    dump.getBytes(buf, len + 1);
    return e.writeBlock(addr, buf, len);
}

void C3OSRecovery() {
    size_t currentHeap = ESP.getFreeHeap();
    uint8_t errorCode = (uint8_t)esp_reset_reason();

    String pCap = "PCAP: <" + crashID() + ">";
    String mill = "MILL: <0x" + String(millis()) + ">";
    String cap = "CAP  : <0x" + String(currentHeap % 1000) + ">";
    String pca = "PC-A: <0x" + String(errorCode) + ">";

    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);
    display.sendBuffer();

    display.drawXBM(1, 1, 9, 8, image_Alert_bits);

    display.setFont(u8g2_font_5x8_tr);
    display.drawStr(13, 9, "C3OS Recovery");
    display.sendBuffer();

    display.drawStr(1, 21, "Checking EEPROM...");
    display.sendBuffer();

    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(0x50);
        if (Wire.endTransmission() == 0) {
            eep.begin();
            if (eepWriteTest) {
                display.drawStr(115, 21, "OK");
                display.sendBuffer();
            }
        }
    }

    display.drawStr(1, 29, "Dumping...");
    display.sendBuffer();
    String dump = createDump(pCap, mill, cap, pca);
    if (dumpToEEPROM(eep, 0, dump)) {
        display.drawStr(115, 29, "OK");
        display.sendBuffer();
    }

    display.drawStr(1, 37, "Checking Device...");
    display.sendBuffer();

    const char* chipModel = ESP.getChipModel();
    uint8_t chipRev = ESP.getChipRevision();

    String chipRevision = String(chipRev);
    String chipModels = String(chipModel);

    if (chipRevision == String(BOARD_REV) || chipModels == String(BOARD_ATTACHED)) {
        display.drawStr(115, 37, "OK");
        display.sendBuffer();
    }

    display.drawStr(1, 45, "Reloading component");
    display.sendBuffer();
    stopAllService();
    delay(500);
    startService();
    display.drawStr(115, 45, "OK");
    display.sendBuffer();

    display.drawStr(1, 63, "Reboot?");
    display.drawStr(105, 63, "WAIT");
    display.sendBuffer();

    btnOK.attachClick([]() { ESP.restart(); });
    while (true) {
        btnOK.tick();

        delay(2);
        yield();
    }

    display.sendBuffer();
}

void showCrashInfo() {
    size_t currentHeap = ESP.getFreeHeap();
    uint8_t errorCode = (uint8_t)esp_reset_reason();

    String pCap = "PCAP: <" + crashID() + ">";
    String mill = "MILL: <0x" + String(millis()) + ">";
    String cap = "CAP  : <0x" + String(currentHeap % 1000) + ">";
    String pca = "PC-A: <0x" + String(errorCode) + ">";

    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    display.drawXBM(0, 0, 11, 11, image_Untitled_1_bits);

    display.setFont(u8g2_font_profont11_tr);
    display.drawStr(13, 10, "C3OS BLSOD");

    display.setFont(u8g2_font_4x6_tr);

    char reset_buffer[64];
    snprintf(reset_buffer, sizeof(reset_buffer), "Reason: %d", esp_reset_reason());

    char heap_buffer[32];
    snprintf(heap_buffer, sizeof(heap_buffer), "Current Heap: %d", currentHeap % 1000);

    display.drawStr(1, 17, reset_buffer);
    display.drawStr(1, 23, heap_buffer);
    display.sendBuffer();

    display.drawStr(1, 63, pCap.c_str());
    display.drawStr(1, 57, mill.c_str());
    display.drawStr(1, 51, cap.c_str());
    display.drawStr(1, 45, pca.c_str());

    display.sendBuffer();
    btnOK.attachClick([]() { isOnErrorFlag = false; delay(50); C3OSRecovery(); });

    while (isOnErrorFlag) {
        btnOK.tick();

        delay(2);
        yield();
    }
}

void enable_pm() {
    esp_pm_config_esp32c3_t pm = {
        .max_freq_mhz = 160,
        .min_freq_mhz = 40,
        .light_sleep_enable = true};

    esp_pm_configure(&pm);
}

void showFinalErrorScreen();
void showSuccessScreen();

void writeEEPROM(uint16_t addr, uint8_t val) {
    Wire.beginTransmission(EEPROM_ADDR);
    Wire.write((addr >> 8) & 0xFF);
    Wire.write(addr & 0xFF);
    Wire.write(val);
    Wire.endTransmission();
    delay(5);
}

uint8_t readEEPROM(uint16_t addr) {
    Wire.beginTransmission(EEPROM_ADDR);
    Wire.write((addr >> 8) & 0xFF);
    Wire.write(addr & 0xFF);
    Wire.endTransmission();

    Wire.requestFrom(EEPROM_ADDR, 1);
    if (Wire.available()) return Wire.read();
    return 0xFF;
}

bool eepromAvailable() {
    Wire.beginTransmission(0x50);
    uint8_t err = Wire.endTransmission();
    return (err == 0);
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
        snprintf(statusBuf, sizeof(statusBuf), "Retry: %d/%d", retryCount, RECONNECT_TIMEOUT / RETRY_INTERVAL);
        display.drawStr(45, 38, statusBuf);

        unsigned long elapsed = millis() - startTime;
        unsigned long remaining = RECONNECT_TIMEOUT - elapsed;
        snprintf(statusBuf, sizeof(statusBuf), "Time left: %lu.%lus", remaining / 1000, (remaining % 1000) / 100);
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
    }

    writeEEPROM(INIT_FLAG_ADDR, 0xAA);
    Serial.println("EEPROM initialized");
}

extern "C" void esp_panic_handler() {
    Serial.println("System on Panic Mode!");
    stopAllService();
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

uint32_t SLEEP_TIMEOUT = 120000;

unsigned long lastActive = 0;
static unsigned long lastTick = 0;
bool isOnSleepMode = false;
volatile bool pendingLockscreen = false;

// mn-i
const char* menuItems[] = {
    "WiFi Scan", "WiFi Connect", "Settings", "Brightness", "Browser",
    "WiFi Sniffer", "Beacon Spammer", "Pong", "Hotspot", "Ruler",
    "Keyboard Cat", "WiFi Storm", "Echo Sniffer", "IR Blaster", "Flappy Bird",
    "Factory Reset", "Check System Integrity", "Online Story", "Heap Monitor",
    "Stopwatch", "BenchMyESP", "Minecraft", "Stardew Valley", "BLE Scan", "C3 Burner",
    "WiFi Telnet", "Doom GL"};
const int menuCount = sizeof(menuItems) / sizeof(menuItems[0]);

const int visibleItems = 6;
const int SCROLL_DURATION = 300;
const int NOTIFICATION_DURATION = 2000;

unsigned long scrollStartTime = 0;
unsigned long notificationStart = 0;

int menuIndex = 0;
int menuScrollOffset = 0;
int targetMenuIndex = 0;
int targetScrollOffset = 0;

bool isScrolling = false;
bool notificationActive = false;
bool isEjected = false;

String notificationText = "";

void initMenuButton();

// Animation
float easeOut(float t) {
    return 1 - pow(1 - t, 2);
}

float easeOutCubic(float t) {
    return 1 - pow(1 - t, 3);
}

float easeInOutCubic(float t) {
    return t < 0.5 ? 4 * t * t * t : 1 - pow(-2 * t + 2, 3) / 2;
}

void IRAM_ATTR onButton() {
    lastActive = millis();
}

void fadeInOLED() {
    display.setPowerSave(0);
    for (int c = 0; c <= 180; c += 3) {
        display.setContrast(c);
        delay(5);
    }
}

void fadeOutOLED() {
    for (int c = 180; c >= 0; c -= 3) {
        display.setContrast(c);
        delay(5);
    }
    display.setPowerSave(1);
}

void animateOLEDOff() {
    display.setPowerSave(0);
    const int W = 128;
    const int H = 64;
    const int START_BAND = 24;
    const int DURATION = 400;

    unsigned long t0 = millis();

    while (true) {
        float t = (millis() - t0) / (float)DURATION;
        if (t > 1.0f) t = 1.0f;
        float e = easeOutCubic(t);

        int band = START_BAND - (int)(START_BAND * e);
        if (band < 1) band = 1;
        int y = (H - band) / 2;

        display.clearBuffer();

        display.drawBox(0, y, W, band);
        display.sendBuffer();

        int c = 180 - (int)(e * 160);
        if (c < 20) c = 20;
        display.setContrast(c);

        if (t >= 1.0f) break;
        delay(12);
    }

    for (int c = 20; c >= 0; c -= 4) {
        display.setContrast(c);
        delay(8);
    }

    display.setPowerSave(1);
}

void enterSleep() {
    animateOLEDOff();
    isOnSleepMode = true;

    stopAllService();

    WiFi.mode(WIFI_OFF);

    gpio_deep_sleep_hold_dis();

    gpio_hold_en((gpio_num_t)BUTTON_OK);
    gpio_wakeup_enable((gpio_num_t)BUTTON_OK, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();

    esp_light_sleep_start();

    gpio_hold_dis((gpio_num_t)BUTTON_OK);

    startService();

    isOnSleepMode = false;
    fadeInOLED();

    pendingLockscreen = true;
    WiFi.mode(WIFI_AP_STA);
    drawMenu();
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

void boot();

// Variabel global pendukung
float currentBoxY = 0;
const int ITEM_HEIGHT = 16;  // Gunakan konstanta agar konsisten

void drawMenuItemsWithOffset(float scrollOffset) {
    const int textOffsetY = 12;
    const int drawCount = 5;

    // Gunakan floor untuk memastikan kita mulai menggambar dari item yang benar
    int intOffset = (int)floor(scrollOffset);
    float fractionalOffset = scrollOffset - (float)intOffset;

    // 1. Logika Box
    float targetBoxY = (menuIndex - scrollOffset) * ITEM_HEIGHT;

    // Jika tidak sedang scroll, paksa Box ke target agar tidak ada sisa lerp
    if (!isScrolling) {
        currentBoxY = targetBoxY;
    } else {
        currentBoxY += (targetBoxY - currentBoxY) * 0.35f;
    }

    display.setDrawColor(1);
    display.drawRBox(0, (int)currentBoxY, 128, ITEM_HEIGHT, 3);

    // 2. Gambar Teks
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
    anrActive = false;
    systemUIActive = true;

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
        // Saat diam, pastikan offset benar-benar pas di angka target
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
        // Tangkap posisi saat ini sebagai titik awal baru
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
    if (ESP.getFreeHeap() < app.minHeap) return

        stopAllService();

    currentApp = index;
    appRunning = true;
    appLastBeat = millis();

    applyCapabilityBoundary(app.caps);
    app.entry();

    appRunning = false;
    currentApp = -1;
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

void drawProgress(float value) {
    display.clearBuffer();

    int x0 = 34, y = 42;
    int x1 = x0 + (int)(59 * value);

    display.drawLine(x0, y, x1, y);
    display.drawXBM(60, 9, 11, 11, image_C3_bits);
    display.sendBuffer();
}

void smoothProgress(float from, float to, int durationMs) {
    unsigned long start = millis();
    while (millis() - start < durationMs) {
        float t = (millis() - start) / (float)durationMs;
        float eased = easeOut(t);
        float val = from + (to - from) * eased;
        drawProgress(val);
        delay(16);
    }

    drawProgress(to);
}

void enterSleep();

void batteryDead() {
    display.clearBuffer();
    display.drawXBM(52, 24, 24, 16, image_battery_10_bits);
    display.sendBuffer();
    delay(2500);

    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);
    display.drawXBM(50, 3, 29, 14, image_FaceNopower_bits);

    display.setFont(u8g2_font_5x7_tr);
    display.drawStr(14, 28, "Power Delivery Error");

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(3, 36, "Brownout was detected, consider");
    display.drawStr(26, 43, "shutdown the device!");

    display.setFont(u8g2_font_5x8_tr);
    display.drawStr(24, 53, "OK: Eme Shutdown");
    display.drawStr(32, 61, "OK2: Continue");

    display.sendBuffer();

    btnOK.attachClick([]() {
        eme_shutdown_run();
    });

    btnOK.attachDoubleClick([]() {
        display.clearBuffer();
        display.setFont(u8g2_font_5x7_tr);
        display.drawStr(12, 35, "Running on safe mode!");
        display.sendBuffer();

        vTaskDelay(1000 / portTICK_PERIOD_MS);

        btStop();
        WiFi.mode(WIFI_OFF);

        runBIOS_SAFE_MODE();
    });
}

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
        for (int i = 1; i < MAX_LINES; i++)
            logBuffer[i - 1] = logBuffer[i];

        logBuffer[MAX_LINES - 1] = text;
    }

    scrollY_Log += LINE_H;
    isScrolling_Log = true;
}

void drawLog() {
    display.clearBuffer();
    display.setFont(u8g2_font_4x6_tr);

    if (isScrolling_Log) {
        scrollY_Log *= 0.75f;
        if (scrollY_Log < 0.5f) {
            scrollY_Log = 0;
            isScrolling_Log = false;
        }
    }

    int visible = SCREEN_H / LINE_H;
    int start = max(0, logCount - visible);

    for (int i = 0; i < visible; i++) {
        int idx = start + i;
        if (idx >= logCount) break;

        int y = (i * LINE_H) - (int)scrollY_Log + LINE_H;
        if (y < 0 || y > SCREEN_H) continue;

        display.drawStr(0, y, logBuffer[idx]);
    }

    display.sendBuffer();
}

void logDelay(int ms) {
    unsigned long t = millis();
    while (millis() - t < ms) {
        drawLog();
        delay(10);
    }
}

// boot
void full_boot() {
    pinMode(BUTTON_ACTION, INPUT_PULLUP);
    esp_log_level_set("*", ESP_LOG_ERROR);

    log("Drawing boot icon");
    display.setBitmapMode(1);
    display.clearBuffer();
    display.drawXBM(60, 9, 11, 11, image_C3_bits);
    display.sendBuffer();
    fadeInOLED();
    delay(20);

    Serial.printf(" [%6lu] DISPLAY DRAW ICON\n", millis());
    Serial.printf("Chip Model : %s\n", ESP.getChipModel());
    Serial.printf("Chip Rev   : %d\n", ESP.getChipRevision());

    log("Checking wake source");

    if (digitalRead(BUTTON_ACTION) == LOW) {
        runBIOS();
        return;
    }

    esp_reset_reason_t cause = esp_reset_reason();
    esp_sleep_wakeup_cause_t wake_cause = esp_sleep_get_wakeup_cause();

    if (wake_cause == ESP_SLEEP_WAKEUP_GPIO) {
        smoothProgress(0.0, 0.5, 500);

        setupESP8266Communication();
        sendCommand("avr32:start");
        delay(100);
        sendCommand("avr32:force-restart");
        delay(500);

        sendCommand("32:start");

        smoothProgress(0.5, 1.0, 100);

        showLockscreen(true);
        drawMenu();
        return;
    }

    if (cause == ESP_RST_PANIC || cause == ESP_RST_TASK_WDT || cause == ESP_RST_WDT) {
        showCrashInfo();
        return;
    }

    if (cause == ESP_RST_BROWNOUT) {
        batteryDead();
        return;
    }

    smoothProgress(0.0, 0.3, 600);

    ledcDetachPin(6);
    pinMode(6, OUTPUT);
    digitalWrite(8, LOW);
    delay(5);

    log("Resetting WiFi stack");
    WiFi.setSleep(false);
    WiFi.mode(WIFI_OFF);
    esp_wifi_stop();
    esp_wifi_deinit();
    smoothProgress(0.3, 0.5, 900);
    log("WiFi reset complete");

    ledcSetup(0, 5000, 6);
    ledcAttachPin(6, 1);

    uint32_t tmp;
    if (loadSleep(tmp)) SLEEP_TIMEOUT = tmp;

    log("[mdloader/info]: VL53 test");
    sensor.begin(0x29);
    sensor.setMode(sensor.eContinuous, sensor.eHigh);
    sensor.start();

    ledcSetup(0, 5000, 8);
    ledcAttachPin(8, 0);

    delay(40);
    Serial.println(sensor.getDistance());
    sensor.stop();
    smoothProgress(0.5, 0.8, 1300);

    log("Starting background services");
    registerService("CPUTemp", BA_CPUTemp, 2048);
    registerService("WiFiCheck", BA_WiFi, 4096);
    registerService("Led", BA_LED, 2048);
    registerService("Heap Allocator", BA_HEAP_ALLOCATOR, 1024);
    registerService("Button Reset Emergency", BA_EME_RESTART_COMBINATION, 1024);
    registerService("BATTERY", BA_BATTERY, 2048);
    smoothProgress(0.8, 0.9, 900);

    log("Finalizing boot");

    log("Init memory fence");
    asm volatile("fence");
    asm volatile("fence iorw, iorw");

    for (int i = 0; i < 1000; i++) asm volatile("nop");

    log("Init CSR");
    asm volatile(
        "csrr t0, mepc \n"
        "csrw mscratch, t0 \n");

    gPower.begin(PowerManager::Balanced);
    gPower.setAutoSleepTimeout(0);
    gPower.setPreSleepCallback([]() { stopAllService(); });
    gPower.setPostWakeCallback([]() { startService(); });
    gPower.setDisplayPowerCallback([](bool on) { if (on) fadeInOLED(); else fadeOutOLED(); });
    smoothProgress(0.9, 1.0, 500);

    Serial.println("I2C Scan...");
    int nDevices = 0;
    for (byte addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.print("Found I2C device at 0x");
            Serial.println(addr, HEX);
            nDevices++;
        }
    }
    Serial.println(nDevices ? "Scan done" : "No I2C devices found");

    // Logging
    display.clearBuffer();

    printLog("Setting up things...");
    logDelay(100);

    printLog("Setting up EEPROM...");
    Wire.beginTransmission(0x50);
    byte error = Wire.endTransmission();

    printLog("Checking EEPROM...");
    if (error == 0) {
        printLog("EEPROM Detected!");
    } else {
        printLog("EEPROM Handshake Fail!");
    }
    logDelay(300);

    printLog("Applying settings");
    setting.loadSettings();
    logDelay(100);

    printLog("Initialize EEPROM");
    if (error == 0) {
        printLog("Waiting EEPROM...");
        initEEPROM();
    } else {
        printLog("EEPROM Init Failed");
        initEEPROM_Failed();
    }
    logDelay(100);

    delay(50);

    log("Boot phase completed!");
    setupESP8266Communication();
    sendCommand("32:start");

    startService();
    delay(100);

    showLockscreen(true);
    drawMenu();
}

void safe_boot() {
    pinMode(BUTTON_ACTION, INPUT_PULLUP);
    esp_log_level_set("*", ESP_LOG_ERROR);
    fadeInOLED();
    display.setFont(u8g2_font_5x8_tr);

    Serial.printf(" [%6lu] DISPLAY DRAW ICON\n", millis());

    Serial.printf("Chip Rev   : %d\n", ESP.getChipRevision());
    display.drawStr(1, 7, "Loaded CHIP_REV");
    display.sendBuffer();
    Serial.printf("Chip Model : %s\n", ESP.getChipModel());
    display.drawStr(1, 15, "Loaded CHIP_MOD");
    display.sendBuffer();

    log("Checking wake source");

    if (digitalRead(BUTTON_ACTION) == LOW) {
        runBIOS();
        return;
    }

    log("Resetting WiFi stack");
    WiFi.mode(WIFI_OFF);
    display.drawStr(1, 23, "Loaded WiFi");
    display.sendBuffer();

    log("Starting background services");
    registerService("Heap Allocator", BA_HEAP_ALLOCATOR, 1024);
    display.drawStr(1, 31, "Loaded Service");
    display.sendBuffer();

    log("Finalizing boot");

    log("Init memory fence");
    asm volatile("fence");
    asm volatile("fence iorw, iorw");
    display.drawStr(1, 39, "Loaded FENCE_IORW");
    display.sendBuffer();

    for (int i = 0; i < 1000; i++) asm volatile("nop");

    log("Init CSR");
    asm volatile(
        "csrr t0, mepc \n"
        "csrw mscratch, t0 \n");

    display.drawStr(1, 47, "Loaded CSRW");
    gPower.begin(PowerManager::Balanced);
    gPower.setAutoSleepTimeout(0);
    gPower.setPreSleepCallback([]() { stopAllService(); });
    gPower.setPostWakeCallback([]() { startService(); });
    gPower.setDisplayPowerCallback([](bool on) { if (on) fadeInOLED(); else fadeOutOLED(); });

    delay(50);
    display.drawStr(1, 55, "Loaded Component");
    display.sendBuffer();

    log("Boot phase completed!");
    setupESP8266Communication();
    sendCommand("32:start");
    display.drawStr(1, 63, "Loaded ESP8266-COM");
    display.sendBuffer();

    startService();
    drawMenu();
}

void boot() {
    if (boot_mode == BOOT_SAFE) {
        boot_mode = BOOT_NORMAL;
        safe_boot();
    } else {
        full_boot();
    }
}

// deep handler
void drawANROverlay() {
    display.setDrawColor(0);
    display.drawBox(0, 0, 128, 64);
    display.setDrawColor(1);

    display.drawFrame(8, 10, 112, 44);

    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(16, 24, "Not Responding");
    display.drawStr(16, 36, appTable[anrApp].name);

    if (anrSelect == 0) display.drawBox(16, 44, 40, 12);
    if (anrSelect == 1) display.drawBox(72, 44, 40, 12);

    display.setDrawColor(anrSelect == 0 ? 0 : 1);
    display.drawStr(20, 53, "Wait");

    display.setDrawColor(anrSelect == 1 ? 0 : 1);
    display.drawStr(76, 53, "Force");

    display.setDrawColor(1);
    display.sendBuffer();
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

    if (appRunning && !anrActive && !systemUIActive) {
        if (millis() - appLastBeat > 2000) {
            anrActive = true;
            anrApp = currentApp;
            anrSelect = 0;
        }
    }

    if (anrActive) {
        if (digitalRead(BUTTON_UP) == LOW || digitalRead(BUTTON_DOWN) == LOW) anrSelect ^= 1;

        if (digitalRead(BUTTON_OK) == LOW) {
            appLastBeat = millis();
            anrActive = false;
        }

        if (digitalRead(BUTTON_ACTION) == LOW) {
            killCurrentApp();
            anrActive = false;
        }

        return;
    }

    vTaskDelay(1);
}