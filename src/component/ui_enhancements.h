#pragma once
#include "../UI/floater.h"
#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>

// Button pin definitions (from config.h)
#define BUTTON_UP_PIN 0
#define BUTTON_DOWN_PIN 1
#define BUTTON_OK_PIN 2
#define BUTTON_ACTION_PIN 3

#include "config/config.h"

namespace UX {

// Haptic feedback simulation using visual pulses
class HapticFeedback {
  private:
    static inline float pulseIntensity = 0.0f;
    static inline unsigned long pulseStart = 0;
    static inline bool isPulsing = false;

  public:
    static void triggerClick() {
        pulseIntensity = 1.0f;
        pulseStart = millis();
        isPulsing = true;
    }

    static void triggerConfirm() {
        pulseIntensity = 1.5f;
        pulseStart = millis();
        isPulsing = true;
    }

    static void triggerError() {
        pulseIntensity = 2.0f;
        pulseStart = millis();
        isPulsing = true;
    }

    static void update() {
        if (!isPulsing) return;

        unsigned long now = millis();
        float elapsed = (now - pulseStart) / 100.0f;

        if (elapsed > 1.0f) {
            isPulsing = false;
            pulseIntensity = 0.0f;
            return;
        }

        float intensity = pulseIntensity * (1.0f - elapsed);

        int radius = (int)(intensity * 3.0f);
        if (radius > 0) {
            display.drawCircle(64, 32, radius, U8G2_DRAW_ALL);
        }
    }
};

// Subtle cursor animation for better focus indication
class AnimatedCursor {
  private:
    static inline float cursorPhase = 0.0f;
    static inline int targetX = 0, targetY = 0;
    static inline int currentX = 0, currentY = 0;

  public:
    static void setTarget(int x, int y) {
        targetX = x;
        targetY = y;
    }

    static void update() {
        cursorPhase += 0.2f;
        if (cursorPhase > 6.2832f) cursorPhase -= 6.2832f;

        currentX += (targetX - currentX) * 0.3f;
        currentY += (targetY - currentY) * 0.3f;

        if (sin(cursorPhase) > 0.5f) {
            display.drawPixel(currentX, currentY);
        }
    }
};

// Button press visual feedback
class ButtonFeedback {
  private:
    static inline bool isUpPressed = false;
    static inline bool isDownPressed = false;
    static inline bool isOkPressed = false;
    static inline bool isActionPressed = false;
    static inline unsigned long pressStart = 0;

  public:
    static void update() {
        bool upPressed = (digitalRead(BUTTON_UP_PIN) == LOW);
        bool downPressed = (digitalRead(BUTTON_DOWN_PIN) == LOW);
        bool okPressed = (digitalRead(BUTTON_OK_PIN) == LOW);
        bool actionPressed = (digitalRead(BUTTON_ACTION_PIN) == LOW);

        if (upPressed) display.drawBox(0, 0, 10, 10);
        if (downPressed) display.drawBox(118, 0, 10, 10);
        if (okPressed) display.drawBox(0, 54, 10, 10);
        if (actionPressed) display.drawBox(118, 54, 10, 10);

        isUpPressed = upPressed;
        isDownPressed = downPressed;
        isOkPressed = okPressed;
        isActionPressed = actionPressed;
    }
};

// Smooth transition effects
class TransitionEffects {
  public:
    static void fadeIn() {
        for (int i = 0; i <= 255; i += 15) {
            display.setContrast(i);
            delay(10);
        }
    }

    static void fadeOut() {
        for (int i = 255; i >= 0; i -= 15) {
            display.setContrast(i);
            delay(10);
        }
    }

    static void popIn(int cx, int cy, int maxR) {
        for (int r = 0; r <= maxR; r += 2) {
            display.drawCircle(cx, cy, r);
            display.sendBuffer();
            delay(10);
        }
    }

    static void slideUp(int distance) {
        // This is a logical transition, actual drawing happens in the caller's loop
        // by applying a Y offset that decreases to 0.
    }

    static void slideInFromRight() {
        // Legacy support stub
    }

    static void slideOutToLeft() {
        // Legacy support stub
    }
};

// Status indicator with smooth animations
class StatusIndicator {
  private:
    static inline float progress = 0.0f;
    static inline float targetProgress = 0.0f;
    static inline unsigned long lastUpdate = 0;

  public:
    static void setProgress(float value) { targetProgress = constrain(value, 0.0f, 1.0f); }

    static void update() {
        unsigned long now = millis();
        if (now - lastUpdate > 16) {
            progress += (targetProgress - progress) * 0.1f;

            int barWidth = (int)(progress * 100);
            display.drawFrame(14, 58, 100, 4);
            if (barWidth > 0) {
                display.drawBox(15, 59, barWidth, 2);
            }

            lastUpdate = now;
        }
    }
};

// Enhanced alert system with animations
class AnimatedAlert {
  public:
    static void showAlert(const char *title, const char *message) {
        for (int y = -64; y <= 10; y += 4) {
            display.clearBuffer();
            display.drawRFrame(10, y, 108, 44, 4);
            display.drawStr(15, y + 15, title);
            display.drawStr(15, y + 30, message);
            display.sendBuffer();
            delay(16);
        }

        unsigned long start = millis();
        while (millis() - start < 2000) {
            float pulse = sin((millis() / 100.0f)) * 2.0f;
            if (pulse > 0) {
                display.drawFrame(8, 8, 112, 48);
            }
            display.sendBuffer();
            delay(16);
        }

        for (int y = 10; y >= -64; y -= 4) {
            display.clearBuffer();
            display.drawRFrame(10, y, 108, 44, 4);
            display.drawStr(15, y + 15, title);
            display.drawStr(15, y + 30, message);
            display.sendBuffer();
            delay(16);
        }
    }
};

// Enhanced input feedback
class InputFeedback {
  public:
    static void showTypingIndicator() {
        static float phase = 0.0f;
        phase += 0.2f;

        int x = 100 + (int)(sin(phase) * 10);
        int y = 50;

        display.drawPixel(x, y);
        display.drawPixel(x + 1, y);
        display.drawPixel(x, y + 1);
    }

    static void showSuccess() {
        for (int size = 0; size < 10; size++) {
            display.drawLine(110, 50, 110 + size, 50 + size);
            display.drawLine(110 + size, 50 + size, 120, 40);
            display.sendBuffer();
            delay(20);
        }
    }

    static void showError() {
        for (int shake = -2; shake <= 2; shake++) {
            display.drawLine(110 + shake, 48, 120 + shake, 58);
            display.drawLine(120 + shake, 48, 110 + shake, 58);
            display.sendBuffer();
            delay(50);
        }
    }
};

// Memory usage indicator
class MemoryMonitor {
  public:
    static void drawMemoryBar() {
        size_t freeHeap = ESP.getFreeHeap();
        size_t totalHeap = ESP.getHeapSize();
        float usage = 1.0f - ((float)freeHeap / (float)totalHeap);

        display.setDrawColor(1);

        int barWidth = (int)(usage * 50);
        display.drawFrame(75, 58, 50, 4);
        if (barWidth > 0) {
            display.drawBox(76, 59, barWidth, 2);
        }

        display.setDrawColor(1);
    }
};

} // namespace UX