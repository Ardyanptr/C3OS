#pragma once
#include "config/config.h"
#include "ui_enhancements.h"
#include <Arduino.h>


namespace UXDemo {

// Demo function to showcase the new UX enhancements
inline void runUXDemo() {
    // Initialize display
    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    // 1. Show haptic feedback on button press
    display.drawStr(10, 20, "Press any button");
    display.drawStr(10, 30, "to see feedback");
    display.sendBuffer();

    // Wait for button press with enhanced feedback
    unsigned long demoStart = millis();
    while (millis() - demoStart < 5000) {
        // Update all UX enhancements
        UX::HapticFeedback::update();
        UX::ButtonFeedback::update();
        UX::AnimatedCursor::update();

        // Check for button presses
        if (digitalRead(BUTTON_UP_PIN) == LOW) {
            UX::HapticFeedback::triggerClick();
            display.clearBuffer();
            display.drawStr(10, 32, "Button UP pressed!");
            display.sendBuffer();
            delay(500);
            break;
        } else if (digitalRead(BUTTON_DOWN_PIN) == LOW) {
            UX::HapticFeedback::triggerConfirm();
            display.clearBuffer();
            display.drawStr(10, 32, "Button DOWN pressed!");
            display.sendBuffer();
            delay(500);
            break;
        } else if (digitalRead(BUTTON_OK_PIN) == LOW) {
            UX::HapticFeedback::triggerError();
            display.clearBuffer();
            display.drawStr(10, 32, "Button OK pressed!");
            display.sendBuffer();
            delay(500);
            break;
        }

        delay(16); // ~60fps
    }

    // 2. Show animated alert
    display.clearBuffer();
    display.drawStr(10, 32, "Showing alert...");
    display.sendBuffer();
    delay(1000);

    UX::AnimatedAlert::showAlert("Demo Alert",
                                 "This is an animated alert with smooth transitions!");

    // 3. Show progress indicator
    display.clearBuffer();
    display.drawStr(10, 20, "Progress Demo");
    display.sendBuffer();
    delay(1000);

    for (float p = 0.0f; p <= 1.0f; p += 0.1f) {
        UX::StatusIndicator::setProgress(p);
        UX::StatusIndicator::update();
        display.sendBuffer();
        delay(200);
    }

    // 4. Show memory monitor
    display.clearBuffer();
    display.drawStr(10, 20, "Memory Monitor");
    UX::MemoryMonitor::drawMemoryBar();
    display.sendBuffer();
    delay(2000);

    // 5. Show input feedback
    display.clearBuffer();
    display.drawStr(10, 20, "Typing indicator");
    for (int i = 0; i < 20; i++) {
        UX::InputFeedback::showTypingIndicator();
        display.sendBuffer();
        delay(100);
    }

    // Show success animation
    UX::InputFeedback::showSuccess();
    display.sendBuffer();
    delay(1000);

    // Show error animation
    UX::InputFeedback::showError();
    display.sendBuffer();
    delay(1000);

    display.clearBuffer();
    display.drawStr(10, 32, "UX Demo Complete!");
    display.sendBuffer();
    delay(2000);
}

// Enhanced lockscreen with new UX features
inline void showEnhancedLockscreen() {
    UX::TransitionEffects::fadeIn();

    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    // Draw enhanced lockscreen background
    display.drawRFrame(10, 10, 108, 44, 4);
    display.drawStr(35, 20, "Enhanced");
    display.drawStr(35, 30, "Lockscreen");
    display.drawStr(20, 40, "Press OK to unlock");

    // Add memory indicator
    UX::MemoryMonitor::drawMemoryBar();

    display.sendBuffer();

    // Wait for unlock with enhanced feedback
    while (true) {
        UX::ButtonFeedback::update();
        UX::AnimatedCursor::update();

        if (digitalRead(BUTTON_OK_PIN) == LOW) {
            UX::HapticFeedback::triggerConfirm();
            UX::TransitionEffects::fadeOut();
            break;
        }

        delay(16);
    }
}

// Enhanced menu with smooth transitions
inline void showEnhancedMenu() {
    UX::TransitionEffects::slideInFromRight();

    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    display.drawStr(10, 20, "Enhanced Menu");
    display.drawStr(10, 30, "1. Demo UX");
    display.drawStr(10, 40, "2. Enhanced Lock");
    display.drawStr(10, 50, "3. Back");

    UX::MemoryMonitor::drawMemoryBar();
    display.sendBuffer();

    // Handle menu selection with enhanced feedback
    while (true) {
        UX::ButtonFeedback::update();
        UX::AnimatedCursor::update();

        if (digitalRead(BUTTON_UP_PIN) == LOW) {
            UX::HapticFeedback::triggerClick();
            // Move cursor up
            UX::AnimatedCursor::setTarget(5, 20);
        } else if (digitalRead(BUTTON_DOWN_PIN) == LOW) {
            UX::HapticFeedback::triggerClick();
            // Move cursor down
            UX::AnimatedCursor::setTarget(5, 30);
        } else if (digitalRead(BUTTON_OK_PIN) == LOW) {
            UX::HapticFeedback::triggerConfirm();
            break;
        }

        delay(16);
    }

    UX::TransitionEffects::slideOutToLeft();
}

} // namespace UXDemo