#include "panic.h"

#include <esp_task_wdt.h>

#include "UI/recovery.h"

static bool panicInProgress = false;
static bool exitPanicSignal = false;

static inline uint32_t get_mcause() {
    uint32_t v;
    asm volatile("csrr %0, mcause" : "=r"(v));
    return v;
}

static inline uint32_t get_mtval() {
    uint32_t v;
    asm volatile("csrr %0, mtval" : "=r"(v));
    return v;
}

void handlePanicExit() {
    exitPanicSignal = true;
}

void panic(PanicCode code, const char* msg) {
    if (panicInProgress) esp_restart();
    panicInProgress = true;

    exitPanicSignal = false;

    uint32_t mcause = get_mcause();
    uint32_t mtval = get_mtval();

    Serial.printf("\n!!! KERNEL PANIC !!!\nCode: %d\nMsg: %s\n", code, msg);

    display.clearBuffer();
    display.setFont(u8g2_font_5x8_tr);
    display.drawFrame(0, 0, 128, 64);

    display.drawStr(5, 12, "C3OS KERNEL PANIC");
    display.drawHLine(0, 15, 128);

    char buf[32];
    snprintf(buf, sizeof(buf), "ERR: %s", msg);
    display.drawStr(5, 28, buf);

    snprintf(buf, sizeof(buf), "MCAUSE: 0x%08X", mcause);
    display.drawStr(5, 38, buf);

    snprintf(buf, sizeof(buf), "FREE HEAP: %u", ESP.getFreeHeap());
    display.drawStr(5, 48, buf);

    display.drawStr(5, 60, "Press OK to Recovery");
    display.sendBuffer();

    btnOK.attachClick([]() { exitPanicSignal = true; });

    uint32_t panicStart = millis();

    while (!exitPanicSignal && millis() - panicStart < 5000) {
        btnOK.tick();
        delay(10);
    }

    Serial.println("Rebooting for recovery...");
    delay(500);
    C3OSRecovery();
    ESP.restart();
}