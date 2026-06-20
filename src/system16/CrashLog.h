#pragma once
#include "system16/AT24C256_EEPROM.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include <rom/rtc.h>

// Reset types and Entry Structure
enum ResetType : uint8_t {
    RESET_UNKNOWN = 0,
    RESET_POWER_ON,
    RESET_BROWNOUT,
    RESET_CRASH_WDT,
    RESET_CRASH_PANIC,
    RESET_DEEP_SLEEP,
    RESET_SOFT
};

struct __attribute__((packed)) CrashEntry {
    uint32_t timestamp;
    uint8_t resetType;
    uint8_t taskId;
    uint16_t pc;
    uint16_t sp;
    bool valid;
};

// Main
class CrashLogger {
  public:
    static const uint16_t ADDR_MAGIC = 0;
    static const uint16_t ADDR_HEAD = 4;
    static const uint16_t ADDR_ENTRIES = 8;
    static const uint8_t MAX_ENTRIES = 40;

    void begin(AT24C256 &eeprom);
    ResetType checkResetReason();
    void logEvent(ResetType type, uint8_t taskId = 0, uint16_t pc = 0, uint16_t sp = 0);
    void printLastEntries(U8G2 &display, int count = 3);
    void printToSerial();
    void clear();
    uint8_t getEntryCount();

  private:
    AT24C256 *eeprom;
    uint8_t writeIndex = 0;
    bool initialized = false;
};