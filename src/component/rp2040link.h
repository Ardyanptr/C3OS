#pragma once
#include <Arduino.h>
#include <Wire.h>

#include "config/RPLinkCommands.h"

#define RP2040_I2C_ADDR         0x42

namespace RP2040Link {

inline bool detect() {
    for (int i = 0; i < 3; i++) {
        Wire.beginTransmission(RP2040_I2C_ADDR);
        if (Wire.endTransmission() == 0) return true;
        delay(50);
    }
    return false;
}

inline bool startBenchmark() {
    Wire.beginTransmission(RP2040_I2C_ADDR);
    Wire.write(RP2040::System::BenchStart);
    return Wire.endTransmission() == 0;
}

// Read and Write custom command
inline bool rp2040Send(const uint8_t* data, int len) {
    Wire.beginTransmission(RP2040_I2C_ADDR);
    for (int i = 0; i < len; i++) Wire.write(data[i]);
    return Wire.endTransmission() == 0;
}

inline bool sendCommand(uint8_t cmd) {
    return rp2040Send(&cmd, 1);
}

inline bool readCommand(uint8_t &cmd) {
    if (Wire.requestFrom(RP2040_I2C_ADDR, (uint8_t)1) != 1)
        return false;

    if (!Wire.available())
        return false;

    cmd = Wire.read();
    return true;
}

inline bool readBenchResult(uint32_t &result) {
    int count = Wire.requestFrom(RP2040_I2C_ADDR, 5);
    if (count != 5) return false;
    uint8_t buf[5];
    for (int i = 0; i < 5; i++) {
        buf[i] = Wire.read();
    }
    if (buf[0] == 0) return false;
    result = (uint32_t)buf[1] | ((uint32_t)buf[2] << 8) |
             ((uint32_t)buf[3] << 16) | ((uint32_t)buf[4] << 24);
    return true;
}

}
