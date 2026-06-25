#pragma once
#include <Arduino.h>
#include <Wire.h>

#ifndef EEPROM_ADDR
#define EEPROM_ADDR     0x50
#endif
#ifndef EEPROM_SIZE
#define EEPROM_SIZE     32768
#endif
#ifndef PAGE_SIZE
#define PAGE_SIZE       32
#endif
#ifndef BLOCK_SIZE
#define BLOCK_SIZE      64
#endif
#ifndef INIT_FLAG_ADDR
#define INIT_FLAG_ADDR  0
#endif

#define SETTINGS_MARKER_ADDR 448
#define SETTINGS_MARKER_VAL  0xBB

bool readLastValidBlock(uint8_t *data, uint8_t len);
bool writeBlockSafe(uint8_t *data, uint8_t len);

extern void writeEEPROM(uint16_t addr, uint8_t val);
extern uint8_t readEEPROM(uint16_t addr);