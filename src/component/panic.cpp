#include "panic.h"

#include <Arduino.h>

#include "icons/icon.h"

bool isOnPanicKernelLock = true;

static inline uint32_t read_mcause() {
    uint32_t v;
    asm volatile("csrr %0, mcause" : "=r"(v));
    return v;
}

static inline uint32_t read_mtval() {
    uint32_t v;
    asm volatile("csrr %0, mtval" : "=r"(v));
    return v;
}

uint32_t mcause = read_mcause();
uint32_t mtval = read_mtval();

void panic(PanicCode code, const char* msg) {
    noInterrupts();

    display.setFontMode(1);
    display.setBitmapMode(1);
    display.drawXBM(0, 0, 57, 9, image_wondrlan_bits);

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(58, 7, ": KRNL PANIC");

    display.drawXBM(120, 11, 6, 46, image_passport_left_bits);
    display.drawXBM(3, 10, 7, 7, image_Pin_star_bits);

    display.setFont(u8g2_font_5x8_tr);
    display.drawStr(12, 17, "KERNEL PANIC");

    display.drawFrame(1, 8, 126, 49);

    char trace_buf[32];

    sprintf(trace_buf, "MCAUSE: 0x%08X", mcause);
    display.drawStr(3, 27, trace_buf);

    sprintf(trace_buf, "MTVAL : 0x%08X", mtval);
    display.drawStr(3, 34, trace_buf);

    char stack_trace_buf[32];

    snprintf(stack_trace_buf, sizeof(stack_trace_buf), "STACK TRACE: 0x%08X", ESP.getFreeHeap());
    display.drawStr(3, 41, stack_trace_buf);

    display.drawStr(1, 63, "Press OK to begin recover!");
    display.sendBuffer();

    btnOK.attachClick([]() { isOnPanicKernelLock = false; delay(100); C3OSRecovery(); });

    while (isOnPanicKernelLock) {
        btnOK.tick();

        delay(2);
        yield();
    }
}