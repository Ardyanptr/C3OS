#pragma once

#include <Arduino.h>
#include "config/config.h"

extern void drawMenu();
extern void appHeartBeat();

void timerStart(uint32_t seconds);
void timerStop();
void timerTick();
bool timerIsRunning();

uint32_t timerGetRemain();
uint32_t timerGetTotal();

void runTimerApp();