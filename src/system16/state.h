#pragma once
#include <Arduino.h>

#include "esp_attr.h"

typedef enum {
    BOOT_NORMAL,
    BOOT_SAFE
} boot_mode_t;

RTC_DATA_ATTR extern boot_mode_t boot_mode;