#ifndef CHIP8_H
#define CHIP8_H

#include "clib/u8g2.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <U8g2lib.h>
#include <esp_task_wdt.h>

class Chip8 {
   public:
    TaskHandle_t _taskHandle = NULL;

    uint8_t memory[4096];
    uint8_t V[16];
    uint16_t I;
    uint16_t pc;
    uint16_t stack[16];
    uint8_t sp;
    uint8_t display[64][32];

    uint8_t delayTimer;
    uint8_t soundTimer;

    char layerNotifyText[20] = "";
    unsigned long notifyTimer = 0;

    bool drawFlag = false;

    volatile uint8_t keypad[16];

    U8G2* oled;

    // ========== CRASH DETECTION VARIABLES ==========
    unsigned long lastInstructionTime;
    unsigned long lastPC;
    int samePCCount;
    int invalidOpcodeCount;
    bool crashDetected;
    unsigned long crashStartTime;
    int cycleCount;
    unsigned long lastCycleLog;

    // Constants
    static constexpr unsigned long INSTRUCTION_TIMEOUT = 500;      // 500ms tanpa instruksi
    static constexpr int MAX_SAME_PC = 100;                        // 100 kali PC sama
    static constexpr int MAX_INVALID_OPCODE = 20;                  // 20 opcode invalid
    static constexpr unsigned long RECOVERY_MODE_DURATION = 1000;  // 1 detik mode recovery
    static constexpr int MIN_CYCLES_PER_SEC = 50;                  // Minimal 50 cycles/detik

    // Watchdog eksternal
    static hw_timer_t* watchdogTimer;
    static bool watchdogTriggered;

    void notifyLayer(const char* name) {
        strncpy(layerNotifyText, name, sizeof(layerNotifyText) - 1);
        notifyTimer = millis();
    }

    void init(U8G2* displayPtr) {
        oled = displayPtr;

        memset(memory, 0, 4096);
        memset(V, 0, 16);
        memset(display, 0, sizeof(display));

        for (int i = 0; i < 16; i++) {
            keypad[i] = 0;
        }

        I = 0;
        sp = 0;
        pc = 0x200;

        // Inisialisasi crash detection
        lastInstructionTime = millis();
        lastPC = 0;
        samePCCount = 0;
        invalidOpcodeCount = 0;
        crashDetected = false;
        crashStartTime = 0;
        cycleCount = 0;
        lastCycleLog = millis();

        uint8_t chip8_fontset[80] = {
            0xF0, 0x90, 0x90, 0x90, 0xF0,  // 0
            0x20, 0x60, 0x20, 0x20, 0x70,  // 1
            0xF0, 0x10, 0xF0, 0x80, 0xF0,  // 2
            0xF0, 0x10, 0xF0, 0x10, 0xF0,  // 3
            0x90, 0x90, 0xF0, 0x10, 0x10,  // 4
            0xF0, 0x80, 0xF0, 0x10, 0xF0,  // 5
            0xF0, 0x80, 0xF0, 0x90, 0xF0,  // 6
            0xF0, 0x10, 0x20, 0x40, 0x40,  // 7
            0xF0, 0x90, 0xF0, 0x90, 0xF0,  // 8
            0xF0, 0x90, 0xF0, 0x10, 0xF0,  // 9
            0xA0, 0xF0, 0xA0, 0xA0, 0xA0,  // A
            0xE0, 0x90, 0xE0, 0x90, 0xE0,  // B
            0xF0, 0x80, 0x80, 0x80, 0xF0,  // C
            0xE0, 0x90, 0x90, 0x90, 0xE0,  // D
            0xF0, 0x80, 0xF0, 0x80, 0xF0,  // E
            0xF0, 0x80, 0xF0, 0x80, 0x80   // F
        };
        memcpy(memory, chip8_fontset, sizeof(chip8_fontset));

        // Setup hardware watchdog timer
        setupWatchdog();
    }

    void setupWatchdog() {
        // Hardware watchdog 3 detik (lebih pendek)
        watchdogTimer = timerBegin(0, 80, true);
        timerAttachInterrupt(watchdogTimer, &onWatchdogTimeout, true);
        timerAlarmWrite(watchdogTimer, 3000000, false);  // 3 detik
        timerAlarmEnable(watchdogTimer);
    }

    static void IRAM_ATTR onWatchdogTimeout() {
        watchdogTriggered = true;
    }

    void feedWatchdog() {
        if (watchdogTimer) {
            timerWrite(watchdogTimer, 0);
            watchdogTriggered = false;
        }
    }

    bool loadROM(const char* path) {
        File file = LittleFS.open(path, "r");

        if (!file) return false;

        size_t fileSize = file.size();
        if (fileSize > 4096 - 0x200) {
            file.close();
            return false;
        }

        file.read(memory + 0x200, fileSize);
        file.close();

        // Reset state
        pc = 0x200;
        sp = 0;
        I = 0;

        memset(V, 0, 16);
        memset(display, 0, sizeof(display));

        delayTimer = 0;
        soundTimer = 0;
        drawFlag = true;

        // Reset crash detection
        lastInstructionTime = millis();
        lastPC = 0;
        samePCCount = 0;
        invalidOpcodeCount = 0;
        crashDetected = false;
        cycleCount = 0;
        esp_task_wdt_reset();

        return true;
    }

    void start() {
        xTaskCreate(
            this->engineTask,
            "Chip8_Engine",
            8192,
            this,
            2,  // Priority lebih tinggi
            &_taskHandle);
    }

    void stop() {
        if (_taskHandle != NULL) {
            vTaskDelete(_taskHandle);
            _taskHandle = NULL;
        }

        if (oled != NULL) {
            oled->clearBuffer();
            oled->sendBuffer();
        }

        memset(display, 0, sizeof(display));

        if (watchdogTimer) {
            timerAlarmDisable(watchdogTimer);
            timerEnd(watchdogTimer);
            watchdogTimer = NULL;
        }
    }

    void setKey(uint8_t key, bool pressed) {
        if (key < 16) {
            keypad[key] = pressed ? 1 : 0;
        }
    }

    uint8_t getKey(uint8_t key) {
        if (key < 16) {
            return keypad[key];
        }
        return 0;
    }

    static void engineTask(void* pvParameter) {
        Chip8* engine = (Chip8*)pvParameter;

        unsigned long lastCycleTime = millis();
        int localCycleCount = 0;

        while (true) {
            esp_task_wdt_reset();

            // Feed watchdog setiap iterasi
            engine->feedWatchdog();

            // Cek watchdog trigger
            if (watchdogTriggered) {
                Serial.println("WATCHDOG: System stalled, resetting emulator!");
                engine->handleCrash();
                watchdogTriggered = false;
            }

            // Monitor cycle rate
            if (millis() - lastCycleTime > 1000) {
                if (localCycleCount < MIN_CYCLES_PER_SEC) {
                    Serial.printf("WARNING: Low cycle rate: %d/sec\n", localCycleCount);
                    engine->checkForHang();
                }

                // Log cycle rate every 10 seconds
                if (millis() - engine->lastCycleLog > 10000) {
                    Serial.printf("CHIP8: %d cycles/sec\n", localCycleCount);
                    engine->lastCycleLog = millis();
                }

                lastCycleTime = millis();
                localCycleCount = 0;
            }

            // Jalankan cycle
            for (int i = 0; i < 8; i++) {  // Naikkan dari 5 ke 8
                if (engine->crashDetected) {
                    engine->recoveryMode();
                } else {
                    engine->cycle();
                    engine->cycleCount++;
                    localCycleCount++;
                }

                // Small delay
                delayMicroseconds(50);
            }

            // Update timer
            if (engine->delayTimer > 0) engine->delayTimer--;
            if (engine->soundTimer > 0) engine->soundTimer--;

            // Render display
            if (engine->drawFlag) {
                engine->renderDisplay();
            }

            vTaskDelay(pdMS_TO_TICKS(16));
        }
    }

    void recoveryMode() {
        if (millis() - crashStartTime > RECOVERY_MODE_DURATION) {
            Serial.println("Attempting recovery...");

            // Reset lengkap
            pc = 0x200;
            sp = 0;
            I = 0;
            memset(V, 0, 16);
            memset(display, 0, sizeof(display));

            crashDetected = false;
            invalidOpcodeCount = 0;
            samePCCount = 0;
            drawFlag = true;

            Serial.println("Recovery complete");
        } else {
            delay(5);
        }
    }

    void checkForHang() {
        // Cek apakah emulator hang
        if (millis() - lastInstructionTime > INSTRUCTION_TIMEOUT) {
            Serial.println("HANG DETECTED: No instruction execution");
            handleCrash();
        }

        // Cek stack pointer
        if (sp >= 16) {
            Serial.println("HANG DETECTED: Stack overflow");
            handleCrash();
        }
    }

    void handleCrash() {
        if (crashDetected) return;  // Hindari recursive crash

        crashDetected = true;
        crashStartTime = millis();

        Serial.println("\n=== CHIP-8 CRASH REPORT ===");
        Serial.printf("Time: %lu ms\n", millis());
        Serial.printf("PC: 0x%04X\n", pc);
        Serial.printf("SP: %d\n", sp);
        Serial.printf("I: 0x%04X\n", I);
        Serial.printf("Same PC count: %d\n", samePCCount);
        Serial.printf("Invalid opcode count: %d\n", invalidOpcodeCount);
        Serial.printf("Total cycles: %d\n", cycleCount);

        // Print V registers
        Serial.println("V registers:");
        for (int i = 0; i < 16; i += 4) {
            Serial.printf("  V%X:%02X V%X:%02X V%X:%02X V%X:%02X\n",
                          i, V[i], i + 1, V[i + 1], i + 2, V[i + 2], i + 3, V[i + 3]);
        }

        // Print stack trace
        Serial.println("Stack trace (16 entries):");
        for (int i = 0; i < sp && i < 16; i++) {
            Serial.printf("  [%d] 0x%04X\n", i, stack[i]);
        }

        // Print last few opcodes
        Serial.println("Last opcodes:");
        for (int i = 0; i < 10; i++) {
            uint16_t addr = pc - 20 + (i * 2);
            if (addr >= 0x200 && addr < 4094) {
                uint16_t op = (memory[addr] << 8) | memory[addr + 1];
                Serial.printf("  0x%04X: 0x%04X\n", addr, op);
            }
        }

        Serial.println("===========================\n");

        // Reset ke state aman
        pc = 0x200;
        sp = 0;
        I = 0;
        memset(V, 0, 16);
        memset(display, 0, sizeof(display));
        drawFlag = true;
    }

    void renderDisplay() {
        if (!oled || crashDetected) return;
        oled->clearBuffer();

        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 64; x++) {
                if (display[x][y]) {
                    oled->drawBox(x * 2, y * 2, 2, 2);
                }
            }
        }

        if (millis() - notifyTimer < 1500) {
            oled->setFont(u8g2_font_5x7_tr);
            int tw = oled->getStrWidth(layerNotifyText);

            oled->setDrawColor(0);
            oled->drawBox(64 - (tw/2) - 2, 52, tw + 4, 10);
            oled->setDrawColor(1);
            oled->drawFrame(64 - (tw/2) - 2, 52, tw + 4, 10);

            oled->drawStr(64 - (tw/2), 60, layerNotifyText);
        }

        // Tampilkan crash indicator
        if (crashDetected) {
            oled->setFont(u8g2_font_5x7_tr);
            oled->setDrawColor(1);
            oled->drawBox(0, 0, 128, 12);
            oled->setDrawColor(0);
            oled->setCursor(10, 8);
            oled->print("RECOVERING...");
            oled->setDrawColor(1);
        }

        oled->sendBuffer();
        drawFlag = false;
    }

    void cycle() {
        esp_task_wdt_reset();

        // Update timestamp
        lastInstructionTime = millis();

        // Validasi PC
        if (pc >= 4094 || pc < 0x200) {
            Serial.printf("Invalid PC: 0x%04X\n", pc);
            pc = 0x200;
            invalidOpcodeCount++;
            return;
        }

        // Deteksi infinite loop
        if (pc == lastPC) {
            samePCCount++;
            if (samePCCount > MAX_SAME_PC) {
                Serial.println("Infinite loop detected");
                handleCrash();
                return;
            }
        } else {
            samePCCount = 0;
            lastPC = pc;
        }

        // Baca opcode
        uint16_t opcode = (memory[pc] << 8) | memory[pc + 1];

        // Validasi opcode
        if (opcode == 0x0000) {
            invalidOpcodeCount++;
            if (invalidOpcodeCount > MAX_INVALID_OPCODE) {
                Serial.println("Too many NOPs");
                handleCrash();
                return;
            }
        } else {
            invalidOpcodeCount = 0;
        }

        pc += 2;

        // Eksekusi opcode
        executeOpcode(opcode);
    }

    void executeOpcode(uint16_t opcode) {
        esp_task_wdt_reset();

        // Proteksi PC
        if (pc >= 4096) {
            pc = 0x200;
            return;
        }

        switch (opcode & 0xF000) {
            case 0x0000:
                if (opcode == 0x00E0) {
                    memset(display, 0, sizeof(display));
                    drawFlag = true;
                } else if (opcode == 0x00EE) {
                    if (sp > 0) {
                        pc = stack[--sp];
                    }
                }
                break;

            case 0x1000:
                pc = opcode & 0x0FFF;
                break;
            case 0x2000:
                if (sp < 16) {
                    stack[sp++] = pc;
                    pc = opcode & 0x0FFF;
                }
                break;
            case 0x3000:
                if (V[(opcode & 0x0F00) >> 8] == (opcode & 0x00FF)) pc += 2;
                break;
            case 0x4000:
                if (V[(opcode & 0x0F00) >> 8] != (opcode & 0x00FF)) pc += 2;
                break;
            case 0x5000:
                if ((opcode & 0x000F) == 0) {
                    if (V[(opcode & 0x0F00) >> 8] == V[(opcode & 0x00F0) >> 4])
                        pc += 2;
                }
                break;
            case 0x6000:
                V[(opcode & 0x0F00) >> 8] = opcode & 0x00FF;
                break;
            case 0x7000:
                V[(opcode & 0x0F00) >> 8] += opcode & 0x00FF;
                break;
            case 0x8000:
                executeArithmetic(opcode);
                break;
            case 0x9000:
                if ((opcode & 0x000F) == 0) {
                    if (V[(opcode & 0x0F00) >> 8] != V[(opcode & 0x00F0) >> 4])
                        pc += 2;
                }
                break;
            case 0xA000:
                I = opcode & 0x0FFF;
                break;
            case 0xB000:
                pc = (opcode & 0x0FFF) + V[0];
                break;
            case 0xC000:
                V[(opcode & 0x0F00) >> 8] = (esp_random() & 0xFF) & (opcode & 0x00FF);
                break;
            case 0xD000:
                executeDraw(opcode);
                break;
            case 0xE000:
                executeKeypad(opcode);
                break;
            case 0xF000:
                executeMemory(opcode);
                break;

            default:
                invalidOpcodeCount++;
                break;
        }
    }

    void executeArithmetic(uint16_t opcode) {
        esp_task_wdt_reset();
        uint8_t x = (opcode & 0x0F00) >> 8;
        uint8_t y = (opcode & 0x00F0) >> 4;
        uint8_t mode = opcode & 0x000F;

        if (x >= 16 || y >= 16) return;

        switch (mode) {
            case 0:
                V[x] = V[y];
                break;
            case 1:
                V[x] |= V[y];
                break;
            case 2:
                V[x] &= V[y];
                break;
            case 3:
                V[x] ^= V[y];
                break;
            case 4: {
                uint16_t sum = V[x] + V[y];
                V[0xF] = (sum > 255) ? 1 : 0;
                V[x] = sum & 0xFF;
            } break;
            case 5: {
                V[0xF] = (V[x] > V[y]) ? 1 : 0;
                V[x] -= V[y];
            } break;
            case 6: {
                V[0xF] = V[y] & 0x1;
                V[x] = V[y] >> 1;
            } break;
            case 7: {
                V[0xF] = (V[y] > V[x]) ? 1 : 0;
                V[x] = V[y] - V[x];
            } break;
            case 0xE: {
                V[0xF] = (V[y] >> 7) & 0x1;
                V[x] = V[y] << 1;
            } break;
        }
    }

    void executeKeypad(uint16_t opcode) {
        esp_task_wdt_reset();
        uint8_t x = (opcode & 0x0F00) >> 8;
        uint8_t mode = opcode & 0x00FF;
        uint8_t key = V[x] & 0xF;

        if (mode == 0x9E) {
            if (keypad[key]) pc += 2;
        } else if (mode == 0xA1) {
            if (!keypad[key]) pc += 2;
        }
    }

    void executeMemory(uint16_t opcode) {
        esp_task_wdt_reset();
        uint8_t x = (opcode & 0x0F00) >> 8;
        uint8_t mode = opcode & 0x00FF;

        if (I >= 4096) I &= 0xFFF;

        switch (mode) {
            case 0x07:
                V[x] = delayTimer;
                break;
            case 0x15:
                delayTimer = V[x];
                break;
            case 0x18:
                soundTimer = V[x];
                break;
            case 0x1E:
                I += V[x];
                break;
            case 0x29:
                I = (V[x] & 0xF) * 5;
                break;
            case 0x33:
                if (I + 2 < 4096) {
                    memory[I] = V[x] / 100;
                    memory[I + 1] = (V[x] / 10) % 10;
                    memory[I + 2] = V[x] % 10;
                }
                break;
            case 0x55:
                for (int i = 0; i <= x && I + i < 4096; i++) {
                    memory[I + i] = V[i];
                }
                break;
            case 0x65:
                for (int i = 0; i <= x && I + i < 4096; i++) {
                    V[i] = memory[I + i];
                }
                break;
            case 0x0A: {
                bool pressed = false;
                for (int i = 0; i < 16; i++) {
                    if (keypad[i]) {
                        V[x] = i;
                        pressed = true;
                        break;
                    }
                }
                if (!pressed) pc -= 2;
            } break;
        }
    }

    void executeDraw(uint16_t opcode) {
        esp_task_wdt_reset();
        uint8_t x = V[(opcode & 0x0F00) >> 8] % 64;
        uint8_t y = V[(opcode & 0x00F0) >> 4] % 32;
        uint8_t height = opcode & 0x000F;

        if (I + height > 4096) return;

        V[0xF] = 0;

        for (int row = 0; row < height; row++) {
            uint8_t spriteByte = memory[I + row];
            uint8_t yPos = (y + row) % 32;

            for (int col = 0; col < 8; col++) {
                if (spriteByte & (0x80 >> col)) {
                    uint8_t xPos = (x + col) % 64;

                    if (display[xPos][yPos] == 1) V[0xF] = 1;
                    display[xPos][yPos] ^= 1;
                }
            }
        }

        drawFlag = true;
    }
};

// Static members definition
hw_timer_t* Chip8::watchdogTimer = NULL;
bool Chip8::watchdogTriggered = false;

#endif