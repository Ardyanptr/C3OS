#pragma once
#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>
#include <WiFi.h>

#include "driver/temp_sensor.h"  // Driver legacy

extern U8G2_SSD1315_128X64_NONAME_F_HW_I2C display;
extern OneButton btnOK;

extern void appHeartBeat();

void runC3Burner();