#pragma once
#include "config/config.h"
#include "ui_demo.h"
#include "ui_enhancements.h"
#include <Arduino.h>

namespace UXIntegration {

// Enhanced version of the existing UI alert system
inline void showEnhancedAlert(const char *title, const char *message_col1, const char *message_col2,
                              const char *message_col3) {
    // Use animated alert instead of static one
    String fullMessage =
        String(message_col1) + " " + String(message_col2) + " " + String(message_col3);
    UX::AnimatedAlert::showAlert(title, fullMessage.c_str());
}

// Enhanced progress display with smooth animations
inline void showEnhancedProgress(const char *title, float progress) {
    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    // Draw enhanced progress frame
    display.drawRFrame(10, 10, 108, 44, 4);
    display.drawStr(15, 20, title);

    // Use smooth progress indicator
    UX::StatusIndicator::setProgress(progress);
    UX::StatusIndicator::update();

    // Add memory monitor
    UX::MemoryMonitor::drawMemoryBar();

    display.sendBuffer();
}

// Enhanced button feedback wrapper
inline void updateEnhancedUI() {
    // Update all UX enhancements in one call
    UX::HapticFeedback::update();
    UX::ButtonFeedback::update();
    UX::AnimatedCursor::update();
    UX::StatusIndicator::update();
}

// Enhanced lockscreen integration
inline void showEnhancedLockscreenWithPet() {
    UX::TransitionEffects::fadeIn();

    // Draw the existing lockscreen but with enhanced elements
    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    // Draw existing lockscreen elements (simplified)
    display.drawRFrame(20, 13, 89, 38, 3);
    display.drawStr(36, 24, "Enhanced Lock");
    display.drawStr(24, 33, "Press OK to unlock");

    // Add memory indicator
    UX::MemoryMonitor::drawMemoryBar();

    display.sendBuffer();

    // Wait for unlock with enhanced feedback
    while (true) {
        updateEnhancedUI();

        if (digitalRead(BUTTON_OK_PIN) == LOW) {
            UX::HapticFeedback::triggerConfirm();
            UX::TransitionEffects::fadeOut();
            break;
        }

        delay(16);
    }
}

// Enhanced menu system
inline void showEnhancedMenuSystem() {
    UX::TransitionEffects::slideInFromRight();

    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    display.drawStr(10, 15, "Enhanced Menu");
    display.drawStr(10, 25, "1. Run Demo");
    display.drawStr(10, 35, "2. Enhanced Lock");
    display.drawStr(10, 45, "3. Progress Test");
    display.drawStr(10, 55, "4. Back");

    // Add animated cursor
    UX::AnimatedCursor::setTarget(5, 25);
    UX::AnimatedCursor::update();

    // Add memory monitor
    UX::MemoryMonitor::drawMemoryBar();

    display.sendBuffer();

    int selection = 0;
    const char *options[] = {"Run Demo", "Enhanced Lock", "Progress Test", "Back"};

    while (true) {
        updateEnhancedUI();

        if (digitalRead(BUTTON_UP_PIN) == LOW) {
            UX::HapticFeedback::triggerClick();
            selection = (selection - 1 + 4) % 4;
            UX::AnimatedCursor::setTarget(5, 25 + (selection * 10));
            delay(200);
        } else if (digitalRead(BUTTON_DOWN_PIN) == LOW) {
            UX::HapticFeedback::triggerClick();
            selection = (selection + 1) % 4;
            UX::AnimatedCursor::setTarget(5, 25 + (selection * 10));
            delay(200);
        } else if (digitalRead(BUTTON_OK_PIN) == LOW) {
            UX::HapticFeedback::triggerConfirm();

            switch (selection) {
            case 0:
                UXDemo::runUXDemo();
                break;
            case 1:
                showEnhancedLockscreenWithPet();
                break;
            case 2:
                for (float p = 0.0f; p <= 1.0f; p += 0.05f) {
                    showEnhancedProgress("Progress Test", p);
                    delay(100);
                }
                break;
            case 3:
                UX::TransitionEffects::slideOutToLeft();
                return;
            }

            // Redraw menu after action
            showEnhancedMenuSystem();
            return;
        }

        delay(16);
    }
}

// Integration wrapper for existing UI functions
namespace LegacyUI {
// Enhanced version of the original showAlert function
inline void showAlert(const char *title, const char *message_col1, const char *message_col2,
                      const char *message_col3) {
    showEnhancedAlert(title, message_col1, message_col2, message_col3);
}

// Enhanced version of progress display
inline void showProgress(float value) { showEnhancedProgress("Loading...", value); }
} // namespace LegacyUI

} // namespace UXIntegration