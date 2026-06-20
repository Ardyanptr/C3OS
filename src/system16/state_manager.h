#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include "config/config.h"

namespace StateManager {

struct SystemState {
    uint32_t magic;       // 0x53544154 (STAT)
    int lastMenuIndex;
    int lastAppIndex;     // -1 if none
    uint8_t dirtyFlag;    // 1 if running, 0 if clean shutdown
    uint32_t checksum;
};

static const char* STATE_FILE = "/cfg/sys_state.bin";

static uint32_t calculateChecksum(const SystemState& state) {
    const uint8_t* p = (const uint8_t*)&state;
    uint32_t sum = 0;
    for (size_t i = 0; i < sizeof(SystemState) - 4; i++) {
        sum += p[i];
    }
    return sum;
}

inline void saveState(int menuIdx, int appIdx, bool dirty) {
    SystemState state;
    state.magic = 0x53544154;
    state.lastMenuIndex = menuIdx;
    state.lastAppIndex = appIdx;
    state.dirtyFlag = dirty ? 1 : 0;
    state.checksum = calculateChecksum(state);
    
    File f = LittleFS.open(STATE_FILE, "w");
    if (f) {
        f.write((uint8_t*)&state, sizeof(SystemState));
        f.close();
    }
}

inline bool loadState(SystemState& state) {
    if (!LittleFS.exists(STATE_FILE)) return false;

    File f = LittleFS.open(STATE_FILE, "r");
    if (!f) return false;

    if (f.read((uint8_t*)&state, sizeof(SystemState)) != sizeof(SystemState)) {
        f.close();
        return false;
    }
    f.close();
    
    if (state.magic != 0x53544154) return false;
    if (state.checksum != calculateChecksum(state)) return false;
    
    return true;
}

} // namespace StateManager
