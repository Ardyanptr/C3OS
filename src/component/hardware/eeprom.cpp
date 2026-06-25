#include <Arduino.h>
#include "eeprom.h"
#include "esp_task_wdt.h"

uint8_t crc8(uint8_t *data, uint8_t len) {
    uint8_t crc = 0;
    for(uint8_t i=0;i<len;i++) {
        crc ^= data[i];
        for(uint8_t j=0;j<8;j++) {
            if(crc & 0x80) crc = (crc << 1) ^ 0x07;
            else crc <<= 1;
        }
    }

    return crc;
}

uint16_t findEmptyBlock() {
    for(uint16_t block=0; block<(EEPROM_SIZE/BLOCK_SIZE); block++) {
        uint8_t marker = readEEPROM(block*BLOCK_SIZE);
        if(marker == 0xFF) return block;
        // Reset watchdog periodically during EEPROM scan
        if(block % 64 == 0) esp_task_wdt_reset();
    }

    return 0xFFFF;
}

bool writeBlockSafe(uint8_t *data, uint8_t len) {
    if(len > BLOCK_SIZE-1) len = BLOCK_SIZE-1;
    uint16_t block = findEmptyBlock();
    if(block == 0xFFFF) block = 0;

    uint16_t addr = block * BLOCK_SIZE;

    uint8_t buffer[BLOCK_SIZE];
    for(uint8_t i=0;i<len;i++) buffer[i] = data[i];
    for(uint8_t i=len;i<BLOCK_SIZE-1;i++) buffer[i] = 0xFF;
    buffer[BLOCK_SIZE-1] = crc8(buffer, BLOCK_SIZE-1);

    for(uint8_t i=0;i<BLOCK_SIZE;i+=PAGE_SIZE) {
        Wire.beginTransmission(EEPROM_ADDR);
        Wire.write((addr+i) >> 8);
        Wire.write((addr+i) & 0xFF);
        Wire.write(&buffer[i], PAGE_SIZE);
        Wire.endTransmission();
        delay(5);
    }

    return true;
}

bool readLastValidBlock(uint8_t *data, uint8_t len) {
    int16_t lastBlock = -1;
    const int16_t totalBlocks = (EEPROM_SIZE/BLOCK_SIZE);

    for(int16_t block=totalBlocks-1; block>=0;block--) {
        uint16_t addr = block * BLOCK_SIZE;
        uint8_t buffer[BLOCK_SIZE];
        for(uint8_t i=0;i<BLOCK_SIZE;i++) buffer[i] = readEEPROM(addr+i);

        uint8_t crcCalc = crc8(buffer, BLOCK_SIZE-1);
        if(crcCalc == buffer[BLOCK_SIZE-1]) {
            lastBlock = block;
            for(uint8_t i=0;i<len && i<BLOCK_SIZE-1;i++) data[i] = buffer[i];
            return true;
        }
        // Reset watchdog periodically during EEPROM scan
        if(block % 64 == 0) esp_task_wdt_reset();
    }

    return false;
}
