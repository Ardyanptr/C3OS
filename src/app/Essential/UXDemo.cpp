#include "UXDemo.h"

static int currentSelection = 0;
static bool inDemo = false;

void runUXDemoApp() {
    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    // Show demo title with enhanced effects
    UX::TransitionEffects::fadeIn();

    display.drawStr(10, 15, "UX Enhancements");
    display.drawStr(10, 25, "Demo Menu");
    display.drawStr(10, 35, "1. Run Full Demo");
    display.drawStr(10, 45, "2. Memory Monitor");
    display.drawStr(10, 55, "3. Button Test");

    // Add memory monitor
    UX::MemoryMonitor::drawMemoryBar();

    // Add animated cursor
    UX::AnimatedCursor::setTarget(5, 35);
    UX::AnimatedCursor::update();

    display.sendBuffer();

    currentSelection = 0;
    inDemo = true;

    // Set up button handlers
    btnUp.attachClick([]() { handleUXDemoInput(INPUT_UP); });
    btnDown.attachClick([]() { handleUXDemoInput(INPUT_DOWN); });
    btnOK.attachClick([]() { handleUXDemoInput(INPUT_OK); });
    btnAction.attachClick([]() { handleUXDemoInput(INPUT_BACK); });

    while (inDemo) {
        // Update all enhanced UI elements
        UX::HapticFeedback::update();
        UX::ButtonFeedback::update();
        UX::AnimatedCursor::update();
        UX::StatusIndicator::update();

        // Handle input
        btnUp.tick();
        btnDown.tick();
        btnOK.tick();
        btnAction.tick();

        delay(16); // ~60fps
    }

    UX::TransitionEffects::fadeOut();
}

void handleUXDemoInput(InputType input) {
    switch (input) {
    case INPUT_UP:
        UX::HapticFeedback::triggerClick();
        currentSelection = (currentSelection - 1 + 3) % 3;
        UX::AnimatedCursor::setTarget(5, 35 + (currentSelection * 10));
        break;

    case INPUT_DOWN:
        UX::HapticFeedback::triggerClick();
        currentSelection = (currentSelection + 1) % 3;
        UX::AnimatedCursor::setTarget(5, 35 + (currentSelection * 10));
        break;

    case INPUT_OK:
        UX::HapticFeedback::triggerConfirm();
        executeSelection();
        break;

    case INPUT_BACK:
        UX::HapticFeedback::triggerError();
        inDemo = false;
        break;
    }
}

void executeSelection() {
    switch (currentSelection) {
    case 0: // Run Full Demo
        UX::TransitionEffects::slideInFromRight();
        UXDemo::runUXDemo();
        UX::TransitionEffects::slideOutToLeft();
        break;

    case 1: // Memory Monitor
        display.clearBuffer();
        display.drawStr(10, 20, "Real-time Memory");
        display.drawStr(10, 30, "Monitor");

        for (int i = 0; i < 100; i++) {
            UX::MemoryMonitor::drawMemoryBar();
            display.sendBuffer();
            delay(100);

            // Simulate memory usage changes
            if (i % 20 == 0) {
                UX::HapticFeedback::triggerClick();
            }
        }
        break;

    case 2: // Button Test
        display.clearBuffer();
        display.drawStr(10, 20, "Button Test");
        display.drawStr(10, 30, "Press any button");
        display.drawStr(10, 40, "to see feedback");

        unsigned long testStart = millis();
        while (millis() - testStart < 3000) {
            UX::ButtonFeedback::update();
            UX::HapticFeedback::update();
            display.sendBuffer();
            delay(16);
        }
        break;
    }

    // Return to main menu
    display.clearBuffer();
    display.drawStr(10, 15, "UX Enhancements");
    display.drawStr(10, 25, "Demo Menu");
    display.drawStr(10, 35, "1. Run Full Demo");
    display.drawStr(10, 45, "2. Memory Monitor");
    display.drawStr(10, 55, "3. Button Test");

    UX::MemoryMonitor::drawMemoryBar();
    UX::AnimatedCursor::setTarget(5, 35 + (currentSelection * 10));
    UX::AnimatedCursor::update();

    display.sendBuffer();
}