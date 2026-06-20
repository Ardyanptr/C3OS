#include "CrashLog.h"
#include "clib/u8g2.h"
#include "system16/AT24C256_EEPROM.h"
#include <Arduino.h>

extern AT24C256 eep;

void CrashLogger::begin(AT24C256 &eep) {
    eeprom = &eep;

    uint8_t magic[4];
    eeprom->readBlock(ADDR_MAGIC, magic, 4);

    if (magic[0] != 0xC3 || magic[1] != 0x05 || magic[2] != 0x00 || magic[3] != 0x01) {
        uint8_t initMagic[4] = {0xC3, 0x05, 0x00, 0x01};
        eeprom->writeBlock(ADDR_MAGIC, initMagic, 4);

        uint8_t head = 0;
        eeprom->writeBlock(ADDR_HEAD, &head, 1);
        writeIndex = 0;

        CrashEntry empty;
        memset(&empty, 0, sizeof(CrashEntry));

        for (int i = 0; i < MAX_ENTRIES; i++) {
            uint16_t addr = ADDR_ENTRIES + (i * sizeof(CrashEntry));
            eeprom->writeBlock(addr, (uint8_t *)&empty, sizeof(CrashEntry));
        }
    } else {
        eeprom->readBlock(ADDR_HEAD, &writeIndex, 1);
    }

    initialized = true;
}

ResetType CrashLogger::checkResetReason() {
    esp_reset_reason_t reason = esp_reset_reason();

    switch (reason) {
    case ESP_RST_BROWNOUT:
        return RESET_BROWNOUT;
    case ESP_RST_TASK_WDT:
    case ESP_RST_INT_WDT:
        return RESET_CRASH_WDT;
    case ESP_RST_PANIC:
        return RESET_CRASH_PANIC;
    case ESP_RST_DEEPSLEEP:
        return RESET_DEEP_SLEEP;
    case ESP_RST_SW:
        return RESET_SOFT;
    case ESP_RST_POWERON:
        return RESET_POWER_ON;
    default:
        return RESET_UNKNOWN;
    }
}

void CrashLogger::logEvent(ResetType type, uint8_t taskId, uint16_t pc, uint16_t sp) {
    if (!initialized) return;

    CrashEntry entry;
    entry.timestamp = millis();
    entry.resetType = (uint8_t)type;
    entry.taskId = taskId;
    entry.pc = pc;
    entry.sp = sp;
    entry.valid = 0xAA;

    uint16_t addr = ADDR_ENTRIES + (writeIndex * sizeof(CrashEntry));
    eeprom->writeBlock(addr, (uint8_t *)&entry, 1);

    writeIndex = (writeIndex + 1) % MAX_ENTRIES;
    eeprom->writeBlock(ADDR_HEAD, &writeIndex, 1);
}

void CrashLogger::printLastEntries(U8G2 &display, int count) {
    display.clearBuffer();
    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(20, 8, "Event Viewer");
    display.drawHLine(0, 10, 128);

    int entriesFound = 0;
    int idx = (writeIndex - 1 + MAX_ENTRIES) % MAX_ENTRIES;

    for (int i = 0; i < MAX_ENTRIES && entriesFound < count; i++) {
        uint16_t addr = ADDR_ENTRIES + (idx * sizeof(CrashEntry));
        CrashEntry entry;
        eeprom->readBlock(addr, (uint8_t *)&entry, sizeof(CrashEntry));

        if (entry.valid == 0xAA) {
            int y = 22 + (entriesFound * 14);

            const char *icon = "?";
            switch ((ResetType)entry.resetType) {
            case RESET_POWER_ON:
                icon = "PON";
                break;
            case RESET_BROWNOUT:
                icon = "BRW";
                break;
            case RESET_CRASH_WDT:
                icon = "WDT";
                break;
            case RESET_CRASH_PANIC:
                icon = "PNC";
                break;
            case RESET_DEEP_SLEEP:
                icon = "SLP";
                break;
            case RESET_SOFT:
                icon = "SFT";
                break;
            default:
                icon = "UNK";
                break;
            }

            display.setCursor(2, y);
            display.print(icon);

            display.setCursor(28, y);
            display.print(entry.timestamp / 1000);
            display.print("s");

            if (entry.resetType == RESET_CRASH_WDT || entry.resetType == RESET_CRASH_PANIC) {
                display.setCursor(60, y);
                display.print("T: ");
                display.print(entry.taskId);
            }

            entriesFound++;
        }
        idx = (idx - 1 + MAX_ENTRIES) % MAX_ENTRIES;
    }

    if (entriesFound == 0) {
        display.drawStr(30, 35, "No events recorded");
    }

    display.sendBuffer();
}

void CrashLogger::clear() {
    CrashEntry empty;
    memset(&empty, 0, sizeof(CrashEntry));

    for (int i = 0; i < MAX_ENTRIES; i++) {
        uint16_t addr = ADDR_ENTRIES + (i * sizeof(CrashEntry));
        eeprom->writeBlock(addr, (uint8_t *)&empty, sizeof(CrashEntry));
    }

    writeIndex = 0;
    eeprom->writeBlock(ADDR_HEAD, &writeIndex, 1);
}

uint8_t CrashLogger::getEntryCount() {
    uint8_t count = 0;

    for (int i = 0; i < MAX_ENTRIES; i++) {
        uint16_t addr = ADDR_ENTRIES + (i * sizeof(CrashEntry));

        CrashEntry entry;

        eeprom->readBlock(addr, (uint8_t *)&entry, sizeof(CrashEntry));

        if (entry.valid == 0xAA) count++;
    }

    return count;
}