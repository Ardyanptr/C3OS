#include "boot.h"

// Helper
void scanI2CBus();
void showSuccessScreen();
void showFinalErrorScreen();
bool eepromAvailable();

uint8_t readEEPROM(uint16_t addr);
void writeEEPROM(uint16_t addr, uint8_t val);

extern uint32_t SLEEP_TIMEOUT;

// EEPROM
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

// I2C Scanner
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

// display add
void fadeInOLED() {
    display.setPowerSave(0);
    for (int c = 0; c <= 180; c += 3) {
        display.setContrast(c);
        delay(5);
    }

    setting.loadSettings();
}

void fadeOutOLED() {
    for (int c = 180; c >= 0; c -= 3) {
        display.setContrast(c);
        delay(5);
    }

    setting.loadSettings();
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

// sleep add
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

// additional
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

    scrollY_Log += LINE_H;
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

    int maxVisible = SCREEN_H / LINE_H;
    int startIndex = max(0, logCount - maxVisible);

    for (int i = 0; i < maxVisible; i++) {
        int idx = startIndex + i;
        if (idx >= logCount) break;

        int yPos = (i * LINE_H) - (int)scrollY_Log + LINE_H;

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

// main
void full_boot() {
    pinMode(BUTTON_ACTION, INPUT_PULLUP);
    esp_log_level_set("*", ESP_LOG_ERROR);

    log("Drawing boot icon");
    display.setBitmapMode(1);
    display.clearBuffer();
    display.drawXBM(60, 9, 11, 11, image_C3_bits);
    display.drawXBM(36, 55, 57, 9, image_wondrlan_bits);
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
        sendCommand("avr32:force-restart");
        delay(50);

        sendCommand("32:start");
        registerService("CPUTemp", BA_CPUTemp, 2048);
        registerService("WiFiCheck", BA_WiFi, 4096);
        registerService("Led", BA_LED, 2048);
        registerService("Heap Allocator", BA_HEAP_ALLOCATOR, 1024);
        registerService("Button Reset Emergency", BA_EME_RESTART_COMBINATION, 1024);
        registerService("BATTERY", BA_BATTERY, 2048);

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

    printLog("Initialize EEPROM");
    if (error == 0) {
        printLog("Waiting EEPROM...");
        initEEPROM();
    } else {
        printLog("EEPROM Init Failed");
        initEEPROM_Failed();
    }
    logDelay(100);

    printLog("Mounting FS");
    if (!LittleFS.begin(true))
        printLog("Failed to mount FS!");
    else
        printLog("LittleFS Mounted!");

    logDelay(200);

    delay(100);

    printLog("Applying settings");
    setting.loadSettings();
    SLEEP_TIMEOUT = Settings::instance->get().sleepTimeout;

    logDelay(100);

    File root = LittleFS.open("/");
    File f = root.openNextFile();
    while (f) {
        Serial.println(f.name());
        f = root.openNextFile();
    }
    root.close();

    runLuaScript("/init.lua");
    runLuaScript("/motd/rc_local.lua");

    delay(100);

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