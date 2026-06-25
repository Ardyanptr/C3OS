#pragma once
#include <math.h>

#include "aod_environment.h"

#include "app/Essential/Timer.h"
#include "component/service.h"
#include "component/ui_enhancements.h"

#include "system16/loader.h"
#include "system16/state_manager.h"

#include "config/config.h"
#include "config/var_declare.h"

#include "driver/rtc_io.h"
#include "esp_sleep.h"
#include <esp_task_wdt.h>

#include "icons/icon.h"
#include "icons/animation.h"

extern void appHeartBeat();
extern bool systemUIActive;
extern uint32_t SLEEP_TIMEOUT;
extern bool menuEntranceRequested;
extern void runApp(int index);

static float flyOffset = 0.0f;
static bool lastTimerState = false;

static Config sysConfig_lock;

inline void displayAOD() {
    btnUp.tick();

    esp_task_wdt_reset();

    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    displayEnvironmentAOD();

    uint32_t now = millis();
    bool isRunning = timerIsRunning();

    if (lastTimerState == true && isRunning == false) {
        flyOffset = 1.0f;
    }
    lastTimerState = isRunning;

    if (isRunning || flyOffset > 0.0f) {
        int yShift = 0;

        if (!isRunning && flyOffset > 0.0f) {
            flyOffset += flyOffset * 0.15f;
            yShift = -(int)flyOffset;
            if (yShift < -70) flyOffset = 0.0f;
        }

        uint32_t t = timerGetRemain();
        uint8_t h = t / 3600;
        uint8_t m = (t % 3600) / 60;
        uint8_t s = t % 60;

        char buf[20];
        sprintf(buf, "%02d:%02d:%02d", h, m, s);

        display.drawRFrame(3, 12 + yShift, 123, 22, 3);
        display.drawXBM(7, 16 + yShift, 7, 7, image_Pin_star_bits);

        display.setFont(u8g2_font_5x8_tr);
        display.drawStr(16, 23 + yShift, "Timer");
        display.drawStr(41, 23 + yShift, buf);

        display.drawLine(6, 28 + yShift, 122, 28 + yShift);
        display.drawLine(6, 26 + yShift, 6, 30 + yShift);
        display.drawLine(122, 26 + yShift, 122, 30 + yShift);

        uint32_t total = timerGetTotal();
        if (total > 0) {
            int progressWidth = (t * 116) / total;
            display.drawBox(6, 27 + yShift, progressWidth, 3);
        }
    }

    display.sendBuffer();
}

inline void startAOD() {
    stopService("Pocket Detection");
    esp_task_wdt_reset();
    yield();

    Serial.println("[AOD] Entering AOD");

    display.setPowerSave(0);
    display.setContrast(1);
    display.setFontMode(1);
    display.setBitmapMode(1);
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(20, 32, "AOD READY");
    display.sendBuffer();

    Serial.println("[AOD] Display sanity check done");

    uint8_t pin = Settings::instance->get().aodPin;
    pinMode(pin, INPUT_PULLUP);

    uint32_t settleStart = millis();
    while (digitalRead(pin) == LOW && millis() - settleStart < 500) {
        yield();
        esp_task_wdt_reset();
        delay(1);
    }

    uint32_t btnPressedAt = 0;
    bool lastBtn = HIGH;

    flyOffset = 0.0f;
    lastTimerState = false;

    uint32_t lastTick = millis();
    uint32_t lastDisplayUpdate = 0;

    btnUp.attachClick([]() { /* PET_FEED */ });
    btnUp.attachDoubleClick([]() { /* PET_SLEEP */ });
    btnUp.attachMultiClick([]() { /* PET_PLAY */ });

    while (true) {
        esp_task_wdt_reset();
        appHeartBeat();

        uint32_t now = millis();
        bool btn = digitalRead(pin);

        static uint32_t lastLog = 0;
        if (now - lastLog > 200) {
            Serial.printf("BTN=%d\n", btn);
            lastLog = now;
        }

        if (lastBtn == HIGH && btn == LOW) {
            btnPressedAt = now;
        }

        if (btn == LOW && btnPressedAt && now - btnPressedAt > 200) {
            Serial.println("[AOD] Exit");
            break;
        }

        if (btn == HIGH) btnPressedAt = 0;
        lastBtn = btn;

        if (now - lastTick >= 1000) {
            timerTick();
            lastTick = now;
        }

        String status = "IDLE";
        bool fastAnim = (flyOffset > 0.0f) || status.startsWith("PLAY") ||
                        status.startsWith("GRUMPY") || status.startsWith("EAT");
        uint32_t refresh = fastAnim ? 33 : 500;

        if (now - lastDisplayUpdate >= refresh) {
            displayAOD();

            // Add enhanced UI feedback during AOD
            UX::ButtonFeedback::update();
            UX::MemoryMonitor::drawMemoryBar();

            lastDisplayUpdate = now;
        }

        vTaskDelay(1);
    }

    startServiceByName("Pocket Detection");
    esp_task_wdt_reset();

    display.setContrast(Settings::instance->get().oledContrast);
}

static const unsigned char image_user_bits[] = {
    0xe0, 0x00, 0x10, 0x01, 0x08, 0x02, 0x08, 0x02, 0x08, 0x02, 0x10, 0x01, 0xe0, 0x00, 0x00, 0x00,
    0xf0, 0x01, 0x0c, 0x06, 0x02, 0x08, 0x02, 0x08, 0x01, 0x10, 0x01, 0x10, 0x01, 0x10, 0xff, 0x1f};

inline void showLockscreen(bool isWake) {
    uint8_t pin = Settings::instance->get().aodPin;

    if (SLEEP_TIMEOUT != Settings::instance->get().sleepTimeout) {
        SLEEP_TIMEOUT = Settings::instance->get().sleepTimeout;

        display.clearBuffer();
        display.setBitmapMode(1);
        display.drawRFrame(20, 13, 89, 38, 3);
        display.drawXBM(24, 17, 9, 8, image_Alert_bits);
        display.setFont(u8g2_font_4x6_tr);
        display.drawStr(36, 24, "Setting Warning");
        display.drawStr(24, 33, "Error: Trying to set");
        display.drawStr(24, 41, "Sleep Timeout!");
        display.sendBuffer();
        delay(2500);
    }

    if (pin != 1 && pin != 2 && pin != 3 && pin != 4) {
        LittleFS.begin(true);
        display.clearBuffer();
        display.setBitmapMode(1);
        display.drawRFrame(20, 13, 89, 38, 3);
        display.drawXBM(24, 17, 9, 8, image_Alert_bits);
        display.setFont(u8g2_font_4x6_tr);
        display.drawStr(36, 24, "Setting Warning");
        display.drawStr(24, 33, "Error: AOD PIN Error");
        display.drawStr(24, 41, "AOD Failure!");
        display.sendBuffer();
        delay(2500);
    }

    systemUIActive = true;
    const uint8_t CODE_LEN = 4;

    uint8_t secret[CODE_LEN];
    uint8_t input[CODE_LEN] = {0, 0, 0, 0};

    uint8_t idx = 0;
    uint8_t curVal = 0;

    // Initialize the user PIN
    for(int i=0;i<4;i++) {
        secret[i] = sysConfig_lock.user_pin[i];
    }

    unsigned long lockStart = millis();
    const unsigned long LOCK_TIMEOUT = 10000;

    float masterYOffset = 64.0f;
    float targetMasterY = 0.0f;

    float boxWidth = 40.0f;
    float curBoxWidth = 40.0f;
    float iconY = 13.0f;
    float curIconY = 13.0f;

    bool isShaking = false;
    unsigned long shakeStart = 0;

    bool upWasLow = (digitalRead(BUTTON_UP) == LOW);
    bool downWasLow = (digitalRead(BUTTON_DOWN) == LOW);
    bool okWasLow = (digitalRead(BUTTON_OK) == LOW);
    bool actWasLow = (digitalRead(BUTTON_ACTION) == LOW);

    bool firstFrame = true;
    while (true) {
        esp_task_wdt_reset();
        appHeartBeat();
        timerTick();
        unsigned long now = millis();

        if (now - lockStart > LOCK_TIMEOUT) {
            startAOD();
            lockStart = millis();
            idx = 0;
            curVal = 0;
            curBoxWidth = 40.0f;
            curIconY = 13.0f;
            masterYOffset = 64.0f;
            continue;
        }

        masterYOffset += (targetMasterY - masterYOffset) * 0.15f;
        curBoxWidth += (boxWidth - curBoxWidth) * 0.3f;
        curIconY += (iconY - curIconY) * 0.2f;

        int offsetX = 0;
        if (isShaking) {
            if (now - shakeStart < 400) {
                offsetX = (int)(sin((now - shakeStart) * 0.05f) * 4.0f);
            } else {
                isShaking = false;
                idx = 0;
                curVal = 0;
            }
        }

        display.clearBuffer();
        display.setBitmapMode(1);
        display.setDrawColor(1);
        
        int drawY_Icon = (int)curIconY + (int)masterYOffset;
        display.drawXBM(58 + offsetX, drawY_Icon, 13, 16, image_user_bits);

        int drawW = (int)curBoxWidth;
        int drawX = 64 - drawW / 2 + offsetX;
        int drawY = 39 + (int)masterYOffset;
        display.drawRBox(drawX, drawY, drawW, 10, 4);

        display.setFont(u8g2_font_5x8_tf);
        display.setDrawColor(0);

        String passStr = "";
        for (int i = 0; i < CODE_LEN; i++) {
            if (i < idx)
                passStr += "* ";
            else if (i == idx) {
                passStr += String(curVal);
                passStr += " ";
            } else
                passStr += "- ";
        }
        passStr.trim();

        int strW = display.getStrWidth(passStr.c_str());
        display.drawStr(64 - strW / 2 + offsetX, drawY + 8, passStr.c_str());

        display.setDrawColor(1);

        if (firstFrame) {
            UX::TransitionEffects::fadeIn();
            firstFrame = false;
        }

        display.sendBuffer();

        if (idx >= CODE_LEN && !isShaking) {
            bool ok = true;
            for (uint8_t i = 0; i < CODE_LEN; i++)
                if (input[i] != secret[i]) ok = false;

            if (ok) {
                // Smooth slide out
                targetMasterY = -64.0f;
                if (abs(masterYOffset - targetMasterY) < 1.0f) {
                    break;
                }
                delay(10);
                continue;
            } else {
                isShaking = true;
                shakeStart = millis();
            }
        }

        if (isShaking) {
            delay(15);
            continue;
        }

        int upL = digitalRead(BUTTON_UP);
        int dnL = digitalRead(BUTTON_DOWN);
        int okL = digitalRead(BUTTON_OK);
        int acL = digitalRead(BUTTON_ACTION);
        bool changed = false;

        if (upL == LOW && !upWasLow) {
            curVal = (curVal + 1) % 10;
            curBoxWidth = 52.0f;
            curIconY = 11.0f;
            changed = true;
        }
        upWasLow = (upL == LOW);

        if (dnL == LOW && !downWasLow) {
            curVal = (curVal + 9) % 10;
            curBoxWidth = 52.0f;
            curIconY = 15.0f;
            changed = true;
        }
        downWasLow = (dnL == LOW);

        if (acL == LOW && !actWasLow) {
            if (idx > 0) {
                idx--;
                curVal = input[idx];
                curBoxWidth = 30.0f;
            }
            changed = true;
        }
        actWasLow = (acL == LOW);

        if (okL == LOW && !okWasLow) {
            input[idx] = curVal;
            idx++;
            curBoxWidth = 58.0f;
            changed = true;

            if (idx < CODE_LEN) {
                curVal = 0;
            }
        }
        okWasLow = (okL == LOW);

        if (changed) lockStart = now;
        delay(15);
    }

    lastActive = millis();
    delay(100);

    // Check for sudden power loss / crash
    StateManager::SystemState state;
    if (StateManager::loadState(state) && state.dirtyFlag == 1) {
        display.setFont(u8g2_font_6x10_tr);
        display.drawStr(44, 25, "Sudden Loss");

        display.setFont(u8g2_font_5x7_tr);
        display.drawStr(44, 34, "Restore session?");

        int choice = 0; // 0=YES, 1=NO
        float selectAnim = (float)choice;
        float selectAnimTarget = (float)choice;

        bool upPrev = (digitalRead(BUTTON_UP) != LOW);
        bool downPrev = (digitalRead(BUTTON_DOWN) != LOW);
        bool okPrev = (digitalRead(BUTTON_OK) != LOW);

        uint32_t lastUiDraw = 0;
        const uint32_t uiFrameIntervalMs = 33;

        uint32_t lastAnimFrame = 0;
        const uint32_t animFrameIntervalMs = 40;

        while (true) {
            esp_task_wdt_reset();
            appHeartBeat();

            display.setFont(u8g2_font_6x10_tr);
            display.drawStr(44, 25, "Sudden Loss");
            display.setFont(u8g2_font_5x7_tr);
            display.drawStr(44, 34, "Restore session?");

            uint32_t now = millis();

            bool upNow = (digitalRead(BUTTON_UP) != LOW);
            bool downNow = (digitalRead(BUTTON_DOWN) != LOW);
            bool okNow = (digitalRead(BUTTON_OK) != LOW);

            // Trigger on PRESS (transition from HIGH to LOW)
            bool upPressed = (upPrev && !upNow);
            bool downPressed = (downPrev && !downNow);
            bool okPressed = (okPrev && !okNow);

            if (upPressed || downPressed) {
                choice ^= 1;
                selectAnimTarget = (float)choice;
            }

            if (okPressed) {
                if (choice == 0) {
                    menuIndex = state.lastMenuIndex;
                    if (state.lastAppIndex != -1) {
                        runApp(state.lastAppIndex);
                        return;
                    }
                }
                break;
            }

            upPrev = upNow;
            downPrev = downNow;
            okPrev = okNow;

            if (now - lastUiDraw >= uiFrameIntervalMs) {
                lastUiDraw = now;
                selectAnim += (selectAnimTarget - selectAnim) * 0.35f;

                // Warning animation frame update
                if (now - lastAnimFrame >= animFrameIntervalMs) {
                    lastAnimFrame = now;
                    animation_warning_blink_32_32_28f_frame = (animation_warning_blink_32_32_28f_frame + 1) % 28;
                }

                display.clearBuffer();

                // Redraw static text
                display.setFont(u8g2_font_6x10_tr);
                display.drawStr(44, 25, "Sudden Loss");
                display.setFont(u8g2_font_5x7_tr);
                display.drawStr(44, 34, "Restore session?");

                // Draw Warning Animation
                display.setDrawColor(0);
                display.drawBox(5, 18, 32, 32);
                display.setDrawColor(1);
                display.drawXBMP(5, 18, 32, 32, animation_warning_blink_32_32_28f_frames[animation_warning_blink_32_32_28f_frame]);

                // Selection UI
                display.setDrawColor(1);
                display.drawRFrame(44, 39, 30, 10, 2);
                display.drawRFrame(79, 39, 30, 10, 2);

                int selectX = 44 + (int)(selectAnim * 35.0f);

                if (selectAnim < 0.5f) {
                    display.drawRBox(selectX, 39, 30, 10, 2);
                    display.setDrawColor(0);
                    display.drawStr(51, 47, "Yes");
                    display.setDrawColor(1);
                    display.drawStr(90, 47, "No");
                } else {
                    display.drawRBox(selectX, 39, 30, 10, 2);
                    display.setDrawColor(1);
                    display.drawStr(51, 47, "Yes");
                    display.setDrawColor(0);
                    display.drawStr(90, 47, "No");
                }
                display.sendBuffer();
            }

            vTaskDelay(1);
            yield();
        }

        StateManager::saveState(menuIndex, -1, false);
    }

    menuEntranceRequested = true;
    drawMenu();
}