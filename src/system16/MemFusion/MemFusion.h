#pragma once
#include <Arduino.h>

#include "app/Essential/Settings.h"

struct UARTLuaReader {
    bool done;
};

inline void
swapPush(int slot, String data) {
    if (Settings::instance->get().memFusion == 0) return;

    Serial.println("mem:push:" + String(slot) + ":" + data);
    Serial1.printf("mem:push:%d:%s\n", slot, data.c_str());
}

inline String swapPull(int slot) {
    if (Settings::instance->get().memFusion == 0) return "FUSION_OFF";

    Serial.printf("mem:pull:%d\n", slot);

    unsigned long timeout = millis();
    while (!Serial1.available() && millis() - timeout < 200);

    if (Serial1.available()) {
        String res = Serial1.readStringUntil('\n');
        res.trim();

        Serial.println(res);
        return res;
    }

    return "TIMEOUT";
}