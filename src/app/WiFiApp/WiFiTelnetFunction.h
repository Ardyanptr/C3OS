#pragma once
#include <Arduino.h>

void tnet_print(const String& s) {
    Serial1.print("to_tnet:");
    Serial1.println(s);
}

inline void cmd_help(const String& arg) {
    tnet_print("help    - show commands");
    tnet_print("status  - system status");
    tnet_print("heap    - free heap");
    tnet_print("reboot  - restart");
}

inline void cmd_status(const String& arg) {
    tnet_print("All systems GO.");
}

inline void cmd_heap(const String& arg) {
    tnet_print(String(ESP.getFreeHeap()));
}

inline void cmd_reboot(const String& arg) {
    tnet_print("Rebooting...");
    delay(200);
    ESP.restart();
}

inline void cmd_neofetch(const String& arg) {
    tnet_print("      ____   ____OS");
    tnet_print("     / ___| | ___ |");
    tnet_print("    | |     | | | |");
    tnet_print("    | |___  | |_| |");
    tnet_print("     \\____| |_____|");
    tnet_print("");

    tnet_print("OS: C3OS v0.1");
    tnet_print("MCU: ESP32-C3");
    tnet_print("Heap Free: " + String(ESP.getFreeHeap()));
    tnet_print("Uptime: " + String(millis() / 1000) + "s");
    tnet_print("Shell: C3OS Shell");
}

inline void cmd_uptime(const String& arg) {
    tnet_print(String(millis() / 1000) + "s");
}

inline void cmd_whoami(const String& arg) {
    tnet_print(String(ESP.getChipModel()));
}

inline void cmd_date(const String& arg) {
    tnet_print(String(__DATE__) + " " + String(__TIME__));
}

inline void cmd_gpio(const String& arg) {
    if (arg.length() == 0) {
        tnet_print("Usage: gpio <pin> [value]");
        return;
    }

    int pin = arg.toInt();
    if (pin < 0 || pin > 22) {
        tnet_print("Invalid Pin!");
        return;
    }

    int space = arg.indexOf(' ');
    if (space > 0) {
        int val = arg.substring(space + 1).toInt();
        pinMode(pin, OUTPUT);
        digitalWrite(pin, val ? HIGH : LOW);
        tnet_print("Pin " + String(pin) + " set to " + String(val ? "HIGH" : "LOW"));
    } else {
        pinMode(pin, INPUT_PULLUP);
        int val = digitalRead(pin);
        tnet_print("Pin " + String(pin) + " is " + String(val ? "HIGH" : "LOW"));
    }
}