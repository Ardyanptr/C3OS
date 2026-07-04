#pragma once
#include <Arduino.h>
#include <stdint.h>

namespace RP2040 {

namespace System {
    constexpr uint8_t Blink             = 0x01;
    constexpr uint8_t BenchStart        = 0x10;
    constexpr uint8_t Reset             = 0x2F;
    constexpr uint8_t DFU               = 0x30;
    constexpr uint8_t EnterBootloader   = 0x31;
}

}