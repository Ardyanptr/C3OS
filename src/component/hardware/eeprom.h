#include <Arduino.h>
#include <Wire.h>

#define EEPROM_ADDR     0x50
#define EEPROM_SIZE     32768
#define PAGE_SIZE       32
#define BLOCK_SIZE      64
#define INIT_FLAG_ADDR  0

bool readLastValidBlock(uint8_t *data, uint8_t len);
bool writeBlockSafe(uint8_t *data, uint8_t len);

extern void writeEEPROM(uint16_t addr, uint8_t val);
extern uint8_t readEEPROM(uint16_t addr);