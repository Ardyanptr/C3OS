#pragma once

// ─────────────────────────────────────────────────────────────
//  Drop-in replacement for handle_crash_recovery()
//  Requires the same crash_magic / crash_task / crash_time
//  globals your existing panic handler already sets.
//
//  Navigation:
//    Page 1 — Panic summary    OK=next  LONG=jump to actions
//    Page 2 — Stack dump       UP/DN=scroll  OK=next
//    Page 3 — Recovery menu    UP/DN=select  OK=confirm
//    Page 4 — Countdown        OK=restart now  LONG=cancel
// ─────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <LittleFS.h>
#include <U8g2lib.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config/config.h"

RTC_NOINIT_ATTR uint32_t crash_magic;
RTC_NOINIT_ATTR uint32_t crash_task; // Stores task name POINTER (not string)
RTC_NOINIT_ATTR uint32_t crash_time;
RTC_NOINIT_ATTR uint32_t crash_pc;

#define CRASH_MAGIC_PENDING 0xDEADBEEF
#define CRASH_MAGIC_HANDLED 0xCAFEBABE
#define HEARTBEAT_TIMEOUT_MS 10000

// Button pins — adjust to match your config
#ifndef BUTTON_OK
#define BUTTON_OK     2
#endif
#ifndef BUTTON_UP
#define BUTTON_UP     0
#endif
#ifndef BUTTON_DOWN
#define BUTTON_DOWN   1
#endif
#ifndef BUTTON_ACTION
#define BUTTON_ACTION 3
#endif

// ─────────────────────────────────────────────────────────────
//  Input helpers (raw digitalRead, no OneButton — crash context)
// ─────────────────────────────────────────────────────────────
static void cr_initButtons() {
    pinMode(BUTTON_OK,     INPUT_PULLUP);
    pinMode(BUTTON_UP,     INPUT_PULLUP);
    pinMode(BUTTON_DOWN,   INPUT_PULLUP);
    pinMode(BUTTON_ACTION, INPUT_PULLUP);
}

enum CrBtn { CR_NONE, CR_OK, CR_OK_LONG, CR_UP, CR_DOWN, CR_ACTION, CR_ACTION_LONG };

static CrBtn cr_readBtn() {
    static uint32_t okHeld = 0, actHeld = 0;

    bool ok     = digitalRead(BUTTON_OK)     == LOW;
    bool up     = digitalRead(BUTTON_UP)     == LOW;
    bool dn     = digitalRead(BUTTON_DOWN)   == LOW;
    bool action = digitalRead(BUTTON_ACTION) == LOW;

    if (ok) {
        if (okHeld == 0) okHeld = millis();
        if (millis() - okHeld > 800) { okHeld = 0; return CR_OK_LONG; }
    } else {
        if (okHeld > 0) { okHeld = 0; return CR_OK; }
    }

    if (action) {
        if (actHeld == 0) actHeld = millis();
        if (millis() - actHeld > 800) { actHeld = 0; return CR_ACTION_LONG; }
    } else {
        if (actHeld > 0 && millis() - actHeld < 800) { actHeld = 0; return CR_ACTION; }
        actHeld = 0;
    }

    if (up) { delay(150); return CR_UP; }
    if (dn) { delay(150); return CR_DOWN; }

    return CR_NONE;
}

static void cr_waitRelease() {
    while (digitalRead(BUTTON_OK) == LOW ||
           digitalRead(BUTTON_UP) == LOW ||
           digitalRead(BUTTON_DOWN) == LOW) {
        delay(10);
        esp_task_wdt_reset();
    }
    delay(80);
}

// ─────────────────────────────────────────────────────────────
//  UI primitives
// ─────────────────────────────────────────────────────────────
static void cr_topBar(const char *title) {
    // inverted bar
    display.setDrawColor(1);
    display.drawBox(0, 0, 128, 11);
    display.setDrawColor(0);
    display.setFont(u8g2_font_helvB08_tr);
    display.drawStr(3, 9, title);
    display.setDrawColor(1);
}

static void cr_bottomHint(const char *hint) {
    display.drawHLine(0, 56, 128);
    display.setFont(u8g2_font_4x6_tr);
    display.setDrawColor(1);
    display.drawStr(2, 63, hint);
}

// ─────────────────────────────────────────────────────────────
//  Stack dump buffer — collected once at crash time
// ─────────────────────────────────────────────────────────────
static const uint8_t DUMP_LINES = 24;
static char cr_dumpLines[DUMP_LINES][22]; // 22 chars per line
static uint8_t cr_dumpCount = 0;

static void cr_collectDump() {
    cr_dumpCount = 0;

    // Reset reason string
    esp_reset_reason_t reason = esp_reset_reason();
    const char *reasonStr = "Unknown";
    if      (reason == ESP_RST_TASK_WDT) reasonStr = "Task WDT";
    else if (reason == ESP_RST_INT_WDT)  reasonStr = "Int WDT";
    else if (reason == ESP_RST_PANIC)    reasonStr = "Panic";
    else if (reason == ESP_RST_BROWNOUT) reasonStr = "Brownout";

    auto addLine = [](const char *s) {
        if (cr_dumpCount < DUMP_LINES) {
            strncpy(cr_dumpLines[cr_dumpCount], s, 21);
            cr_dumpLines[cr_dumpCount][21] = '\0';
            cr_dumpCount++;
        }
    };

    char buf[22];

    snprintf(buf, sizeof(buf), "Reason: %s", reasonStr);    addLine(buf);
    snprintf(buf, sizeof(buf), "Uptime: %lums", crash_time); addLine(buf);
    snprintf(buf, sizeof(buf), "Magic:  0x%08X", crash_magic); addLine(buf);
    snprintf(buf, sizeof(buf), "Task:   0x%08X", crash_task);  addLine(buf);
    addLine("--- Stack ---");

    // Stack dump
    register uint32_t sp asm("sp");
    snprintf(buf, sizeof(buf), "SP: 0x%08X", sp); addLine(buf);

    for (int i = 0; i < 16 && cr_dumpCount < DUMP_LINES; i++) {
        uint32_t *addr = (uint32_t *)(sp + i * 4);
        if ((uint32_t)addr > 0x3FC00000 && (uint32_t)addr < 0x3FE00000) {
            snprintf(buf, sizeof(buf), "[%02d] 0x%08X", i, *addr);
            addLine(buf);
        }
    }

    // Also log to serial
    Serial.println("\n========== CRASH DUMP ==========");
    for (uint8_t i = 0; i < cr_dumpCount; i++)
        Serial.println(cr_dumpLines[i]);
    Serial.println("=================================\n");
}

// Save compact crash log to LittleFS (non-destructive)
static void cr_saveLog() {
    if (!LittleFS.begin(false)) return;
    if (!LittleFS.exists("/cfg")) LittleFS.mkdir("/cfg");

    File f = LittleFS.open("/cfg/crash.log", "a"); // append
    if (!f) return;

    f.printf("--- crash @ %lums ---\n", crash_time);
    for (uint8_t i = 0; i < cr_dumpCount; i++) {
        f.println(cr_dumpLines[i]);
    }
    f.println();
    f.close();
}

// ─────────────────────────────────────────────────────────────
//  RCPM force-bootloader helper
// ─────────────────────────────────────────────────────────────
static void cr_forceBootloader() {
    if (LittleFS.begin(false)) {
        if (!LittleFS.exists("/cfg")) LittleFS.mkdir("/cfg");
        File f = LittleFS.open("/cfg/force_bootloader", "w");
        if (f) { f.print("1"); f.close(); }
    }
    esp_restart();
}

// ─────────────────────────────────────────────────────────────
//  Page 1 — Panic summary
// ─────────────────────────────────────────────────────────────
static bool cr_page1() {
    // Decode task name
    const char *taskName = "unknown";
    if (crash_task > 0x3FC00000 && crash_task < 0x3FE00000) {
        const char *n = pcTaskGetTaskName((TaskHandle_t)crash_task);
        if (n) taskName = n;
    }

    esp_reset_reason_t reason = esp_reset_reason();
    const char *reasonStr = "Unknown";
    if      (reason == ESP_RST_TASK_WDT) reasonStr = "Task WDT";
    else if (reason == ESP_RST_INT_WDT)  reasonStr = "Int WDT";
    else if (reason == ESP_RST_PANIC)    reasonStr = "Panic";
    else if (reason == ESP_RST_BROWNOUT) reasonStr = "Brownout";

    display.clearBuffer();
    cr_topBar("KERNEL PANIC");

    display.setFont(u8g2_font_4x6_tr);
    char buf[28];
    snprintf(buf, sizeof(buf), "Task:   %.16s", taskName);
    display.drawStr(2, 21, buf);
    snprintf(buf, sizeof(buf), "Reason: %s", reasonStr);
    display.drawStr(2, 29, buf);
    snprintf(buf, sizeof(buf), "Uptime: %lums", crash_time);
    display.drawStr(2, 37, buf);
    snprintf(buf, sizeof(buf), "Code:   0x%04X", (uint16_t)(crash_magic & 0xFFFF));
    display.drawStr(2, 45, buf);

    cr_bottomHint("OK=dump  LONG=actions");
    display.sendBuffer();

    cr_waitRelease();
    while (true) {
        esp_task_wdt_reset();
        delay(30);
        CrBtn b = cr_readBtn();
        if (b == CR_ACTION)  cr_forceBootloader();
        if (b == CR_OK)      return false; // go to dump
        if (b == CR_OK_LONG) return true;  // skip to actions
    }
}

// ─────────────────────────────────────────────────────────────
//  Page 2 — Scrollable stack dump
// ─────────────────────────────────────────────────────────────
static void cr_page2() {
    const uint8_t VISIBLE = 6; // lines visible at once (64-11-8=45px / 7px)
    uint8_t scroll = 0;

    auto redraw = [&]() {
        display.clearBuffer();
        cr_topBar("STACK DUMP");
        display.setFont(u8g2_font_4x6_tr);

        for (uint8_t i = 0; i < VISIBLE; i++) {
            uint8_t idx = scroll + i;
            if (idx >= cr_dumpCount) break;
            display.drawStr(2, 19 + i * 7, cr_dumpLines[idx]);
        }

        // scrollbar
        if (cr_dumpCount > VISIBLE) {
            uint8_t barH = (VISIBLE * 44) / cr_dumpCount;
            uint8_t barY = 12 + (scroll * 44) / cr_dumpCount;
            display.drawBox(125, barY, 2, barH);
        }

        cr_bottomHint("UP/DN=scroll  OK=next");
        display.sendBuffer();
    };

    redraw();
    cr_waitRelease();

    while (true) {
        esp_task_wdt_reset();
        delay(30);
        CrBtn b = cr_readBtn();
        if (b == CR_ACTION)  cr_forceBootloader();
        if (b == CR_UP   && scroll > 0)                          { scroll--; redraw(); }
        if (b == CR_DOWN && scroll + VISIBLE < cr_dumpCount)     { scroll++; redraw(); }
        if (b == CR_OK)  break;
    }
}

// ─────────────────────────────────────────────────────────────
//  Page 3 — Recovery action menu
// ─────────────────────────────────────────────────────────────
enum CrAction { CR_ACT_RESTART, CR_ACT_SAFE_BOOT, CR_ACT_FORMAT_FS, CR_ACT_BIOS, CR_ACT_COUNT };

static const char *CR_ACT_LABELS[CR_ACT_COUNT]  = { "Restart",   "Safe boot", "Format FS", "BIOS"      };
static const char *CR_ACT_DESCS[CR_ACT_COUNT]   = { "cold reboot","skip svc",  "wipe + fmt", "bios mode" };

static CrAction cr_page3() {
    uint8_t sel = 0;

    auto redraw = [&]() {
        display.clearBuffer();
        cr_topBar("RECOVERY");
        display.setFont(u8g2_font_4x6_tr);

        for (uint8_t i = 0; i < CR_ACT_COUNT; i++) {
            uint8_t y = 13 + i * 11;
            if (i == sel) {
                display.drawBox(0, y, 128, 10);
                display.setDrawColor(0);
            } else {
                display.setDrawColor(1);
            }
            display.drawStr(3, y + 8, CR_ACT_LABELS[i]);
            display.setDrawColor(1);
            display.setFont(u8g2_font_4x6_tr);
            // right-align description
            uint8_t dw = display.getStrWidth(CR_ACT_DESCS[i]);
            if (i != sel) {
                display.setDrawColor(1);
                // dim — draw at reduced contrast by using draw color trick
                display.drawStr(128 - dw - 2, y + 8, CR_ACT_DESCS[i]);
            }
        }

        cr_bottomHint("UP/DN=sel  OK=confirm");
        display.sendBuffer();
    };

    redraw();
    cr_waitRelease();

    while (true) {
        esp_task_wdt_reset();
        delay(30);
        CrBtn b = cr_readBtn();
        if (b == CR_ACTION)  cr_forceBootloader();
        if (b == CR_UP   && sel > 0)              { sel--; redraw(); }
        if (b == CR_DOWN && sel < CR_ACT_COUNT-1) { sel++; redraw(); }
        if (b == CR_OK)  return (CrAction)sel;
    }
}

// ─────────────────────────────────────────────────────────────
//  Page 4 — Restart countdown
// ─────────────────────────────────────────────────────────────
static bool cr_page4(const char *actionLabel) {
    const uint8_t COUNTDOWN = 5;

    for (int8_t t = COUNTDOWN; t >= 0; t--) {
        display.clearBuffer();
        cr_topBar("RESTARTING");

        display.setFont(u8g2_font_4x6_tr);
        display.drawStr((128 - display.getStrWidth(actionLabel)) / 2, 22, actionLabel);

        // big countdown number
        display.setFont(u8g2_font_helvB24_tr);
        char tBuf[4];
        snprintf(tBuf, sizeof(tBuf), "%d", t);
        uint8_t tw = display.getStrWidth(tBuf);
        display.drawStr((128 - tw) / 2, 50, tBuf);

        cr_bottomHint("OK=now  LONG=cancel");
        display.sendBuffer();

        // Poll for 1 second
        uint32_t start = millis();
        while (millis() - start < 1000) {
            esp_task_wdt_reset();
            delay(30);
            CrBtn b = cr_readBtn();
            if (b == CR_ACTION)  cr_forceBootloader();
            if (b == CR_OK)      return true;  // restart now
            if (b == CR_OK_LONG) return false; // cancel → back to menu
        }
    }
    return true; // countdown expired → restart
}

// ─────────────────────────────────────────────────────────────
//  Execute chosen action
// ─────────────────────────────────────────────────────────────
static void cr_executeAction(CrAction act) {
    // Remove crash metrics to prevent duplicate safe-mode diag screen
    if (LittleFS.begin(false) && LittleFS.exists("/cfg/crash_metrics.json")) {
        LittleFS.remove("/cfg/crash_metrics.json");
    }

    switch (act) {
    case CR_ACT_RESTART:
        // default — just fall through to esp_restart()
        break;

    case CR_ACT_SAFE_BOOT:
        // Write a flag so boot() picks BOOT_SAFE
        if (LittleFS.begin(false)) {
            File f = LittleFS.open("/cfg/boot_flag", "w");
            if (f) { f.print("safe"); f.close(); }
        }
        break;

    case CR_ACT_FORMAT_FS:
        // Confirm once more on-screen
        display.clearBuffer();
        cr_topBar("FORMAT FS");
        display.setFont(u8g2_font_4x6_tr);
        display.drawStr(10, 28, "All data will be lost!");
        display.drawStr(10, 38, "OK=confirm  UP=cancel");
        display.sendBuffer();
        cr_waitRelease();
        {
            bool confirmed = false;
            uint32_t t0 = millis();
            while (millis() - t0 < 8000) {
                esp_task_wdt_reset();
                delay(30);
                CrBtn b = cr_readBtn();
                if (b == CR_OK) { confirmed = true; break; }
                if (b == CR_UP) break;
            }
            if (confirmed) {
                LittleFS.begin(false);
                LittleFS.format();
            }
        }
        break;

    case CR_ACT_BIOS:
        // Your existing runBIOS() — declare extern if needed
        extern void runBIOS();
        runBIOS();
        return; // runBIOS() handles its own loop, don't restart after
    
    default:
        break;
    }

    crash_magic = 0;
    esp_restart();
}

// ─────────────────────────────────────────────────────────────
#include "app/CrashApp/CrashEngine.h"

//  Entry point — replace handle_crash_recovery() with this
// ─────────────────────────────────────────────────────────────
void handle_crash_recovery() {
    crash_magic = CRASH_MAGIC_HANDLED;
    cr_initButtons();

    // Collect dump data once (also logs to Serial + LittleFS)
    cr_collectDump();
    
    // Also save to CrashEngine's JSON format for Safe-Mode
    const char *taskName = "unknown";
    if (crash_task > 0x3FC00000 && crash_task < 0x3FE00000) {
        const char *n = pcTaskGetTaskName((TaskHandle_t)crash_task);
        if (n) taskName = n;
    }
    
    CrashLog log;
    strncpy(log.appName, taskName, 31);
    log.appName[31] = '\0';
    log.freeHeap = ESP.getFreeHeap();
    log.resetReason = (uint8_t)esp_reset_reason();
    log.errorCode = crash_pc;
    log.timestamp = crash_time;
    log.valid = true;
    
    // We can't call CrashEngine::saveLog because it's private, 
    // but we can call CrashEngine::captureCrash if we want to reboot, 
    // or we just let cr_saveLog() do its thing.
    // Let's just use CrashEngine's public API if possible.
    // Actually, I'll just make saveLog public or add a method.
    
    CrashEngine::saveLog(log);
    
    cr_saveLog();
    
    // Page 1 — summary
    // Returns true if user long-pressed to skip dump
    bool skipDump = cr_page1();

    // Page 2 — dump (skippable)
    if (!skipDump) cr_page2();

    // Page 3 — actions (loop until valid selection + confirmed)
    CrAction chosen = CR_ACT_RESTART;
    while (true) {
        chosen = cr_page3();

        // Page 4 — countdown confirm
        bool confirmed = cr_page4(CR_ACT_LABELS[chosen]);
        if (confirmed) break;
        // cancelled — loop back to action menu
    }

    // Execute
    cr_executeAction(chosen);
}