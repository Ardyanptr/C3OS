#pragma once
#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>

extern U8G2_SSD1315_128X64_NONAME_F_HW_I2C display;
extern OneButton btnUp;
extern OneButton btnDown;
extern OneButton btnOK;
extern OneButton btnAction;

extern void C3OSRecovery();

enum PanicCode {
    PANIC_I2C_FAIL,
    PANIC_TOF_FAIL,
    PANIC_MEM_CORRUPT,
    PANIC_STACK_OVERFLOW,
    PANIC_IPC_DROP,
    PANIC_TOF_TIMEOUT,
    PANIC_UNKNOWN
};

void panic(PanicCode code, const char* msg);