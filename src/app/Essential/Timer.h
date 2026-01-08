#pragma once

#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>

#include "config/config.h"

extern U8G2_SSD1315_128X64_NONAME_F_HW_I2C display;
extern OneButton btnOK;
extern OneButton btnAction;

extern void drawMenu();
extern void appHeartBeat();

void timerStart(uint32_t seconds);
void timerStop();
void timerTick();
bool timerIsRunning();

uint32_t timerGetRemain();
uint32_t timerGetTotal();

void runTimerApp();