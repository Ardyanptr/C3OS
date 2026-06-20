#pragma once

#include <Arduino.h>
#include "component/ui_demo.h"
#include "component/ui_enhancements.h"
#include "component/ui_integration.h"
#include "config/config.h"

extern void drawMenu();
extern void appHeartBeat();

// Input types for the demo
enum InputType { INPUT_UP, INPUT_DOWN, INPUT_OK, INPUT_BACK };

// Simple demo functions
void runUXDemoApp();
void showUXDemoMenu();
void handleUXDemoInput(InputType input);
void executeSelection();
