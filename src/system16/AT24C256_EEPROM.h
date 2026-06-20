#pragma once
#include <Arduino.h>
#include <Wire.h>

class AT24C256 {
   public:
    AT24C256(uint8_t address = 0x50) : addr(address) {}

    void begin() {
        Wire.begin();
    }

    bool writeBlock(uint16_t memAddr, const uint8_t* data, uint16_t len) {
        while (len > 0) {
            uint8_t chunk = min(len, (uint16_t)64);

            Wire.beginTransmission(addr);
            Wire.write(memAddr >> 8);
            Wire.write(memAddr & 0xFF);

            for (uint8_t i = 0; i < chunk; i++)
                Wire.write(data[i]);

            uint8_t err = Wire.endTransmission();
            if (err != 0) return false;

            delay(5);

            memAddr += chunk;
            data += chunk;
            len -= chunk;
        }
        return true;
    }

    bool readBlock(uint16_t memAddr, uint8_t* data, uint16_t len) {
        Wire.beginTransmission(addr);
        Wire.write(memAddr >> 8);
        Wire.write(memAddr & 0xFF);
        if (Wire.endTransmission() != 0) return false;

        uint16_t read = 0;
        while (read < len) {
            uint8_t chunk = min((int)(len - read), 32);
            Wire.requestFrom(addr, chunk);
            for (uint8_t i = 0; i < chunk && Wire.available(); i++) {
                data[read++] = Wire.read();
            }
        }

        return true;
    }

   private:
    uint8_t addr;
};
