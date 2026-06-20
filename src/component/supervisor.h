#pragma once
#include <Arduino.h>

typedef void (*AppEntry)();

bool supervisedRun(AppEntry entry, const char* name, uint32_t timeoutMs = 5000);