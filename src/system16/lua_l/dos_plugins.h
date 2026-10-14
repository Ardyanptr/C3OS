#pragma once
#include <Arduino.h>

namespace DOSPlugins {
    int  load();
    bool run(const String& cmdline);
    void list();
    size_t count();
}