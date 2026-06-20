#pragma once
#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>

#include "config/config.h"

extern void C3OSRecovery();

enum PanicCode {
    PANIC_I2C_FAIL,
    PANIC_TOF_FAIL,
    PANIC_MEM_CORRUPT,
    PANIC_STACK_OVERFLOW,
    PANIC_IPC_DROP,
    PANIC_TOF_TIMEOUT,
    PANIC_LFS_MOUNT_FAILED,
    PANIC_LUA_ERROR,
    PANIC_UNKNOWN
};

void panic(PanicCode code, const char* msg);