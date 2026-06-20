#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFi.h>

#include "regis.h"
#include "service.h"

#include "config/config.h"
extern SemaphoreHandle_t i2cMutex;

#include "driver/temp_sensor.h"

void BA_CPUTemp(void* param);
void BA_WiFi(void* param);
void BA_EEPROM(void* param);
void BA_LED(void* param);
void BA_HEAP_ALLOCATOR(void* param);
void BA_EME_RESTART_COMBINATION(void* param);
void BA_BATTERY(void* param);
void BA_TIMERTICK(void* param);
void BA_POCKET_DETECTION(void* param);
void BA_ESP8266_CHECK(void* param);