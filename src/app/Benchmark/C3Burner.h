#pragma once
#include "config/config.h"
#include <WiFi.h>

#include "driver/temp_sensor.h" // Driver legacy

extern void appHeartBeat();
extern void drawMenu();

void runC3Burner();