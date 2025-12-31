#pragma once
#include "config/config.h"

extern void appHeartBeat();
extern bool systemUIActive;

inline void showLockscreen(bool isWake) {
    systemUIActive = true;

    const uint8_t CODE_LEN = 4;
    const uint8_t secret[CODE_LEN] = {0, 0, 0, 0};

    uint8_t input[CODE_LEN] = {0, 0, 0, 0};
    uint8_t idx = 0;
    uint8_t curVal = 0;

    unsigned long lastAnim = 0;
    uint8_t phase = 0;
    unsigned long lockStart = millis();
    const unsigned long LOCK_TIMEOUT = 30000;

    auto draw = [&](bool flashError) {
        display.clearBuffer();
        display.setFont(u8g2_font_6x10_tr);
        display.drawStr(2, 12, isWake ? "Ardyan - Enter PIN" : "Ardyan - Enter PIN");

        int startX = 20;
        for (uint8_t i = 0; i < CODE_LEN; ++i) {
            int x = startX + i * 22;
            display.drawRFrame(x, 44, 18, 14, 2);
            if (i < idx) {
                display.setFont(u8g2_font_6x10_tr);
                display.drawStr(x + 6, 54, "*");
            } else if (i == idx) {
                if (!(flashError && ((phase & 1) == 0))) {
                    char ch[2] = {(char)('0' + curVal), 0};
                    display.setFont(u8g2_font_6x10_tr);
                    display.drawStr(x + 6, 54, ch);
                }
            }
        }
        display.sendBuffer();
    };

    bool upWasLow = false, downWasLow = false, okWasLow = false, actWasLow = false;

    while (true) {
        appHeartBeat();

        unsigned long now = millis();
        if (now - lastAnim > 120) {
            phase++;
            lastAnim = now;
        }

        if (isWake && (now - lockStart > LOCK_TIMEOUT)) {
            enterSleep();
            return;
        }

        int upL = digitalRead(BUTTON_UP);
        int dnL = digitalRead(BUTTON_DOWN);
        int okL = digitalRead(BUTTON_OK);
        int acL = digitalRead(BUTTON_ACTION);

        bool changed = false;
        bool flashError = false;

        if (upL == LOW && !upWasLow) {
            curVal = (curVal + 1) % 10;
            changed = true;
            lockStart = now;
        }
        upWasLow = (upL == LOW);

        if (dnL == LOW && !downWasLow) {
            curVal = (curVal + 9) % 10;
            changed = true;
            lockStart = now;
        }
        downWasLow = (dnL == LOW);

        if (acL == LOW && !actWasLow) {
            if (idx > 0) {
                idx--;
                curVal = input[idx];
            }
            changed = true;
            lockStart = now;
        }
        actWasLow = (acL == LOW);

        if (okL == LOW && !okWasLow) {
            input[idx] = curVal;
            idx++;
            lockStart = now;
            if (idx >= CODE_LEN) {
                bool ok = true;
                for (uint8_t i = 0; i < CODE_LEN; ++i)
                    if (input[i] != secret[i]) ok = false;
                if (ok) {
                    digitalWrite(upL, HIGH);
                    digitalWrite(dnL, HIGH);
                    digitalWrite(okL, HIGH);
                    digitalWrite(acL, HIGH);
                    delay(100);
                    break;
                } else {
                    idx = 0;
                    curVal = 0;
                    flashError = true;

                    for (uint8_t k = 0; k < 6; ++k) {
                        phase++;
                        draw(true);
                        delay(60);
                    }
                }
            } else {
                curVal = input[idx];
            }
            changed = true;
        }
        okWasLow = (okL == LOW);

        if (changed) {
            draw(false);
        } else if (now - lastAnim <= 5) {
            draw(false);
        }

        delay(10);
    }

    lastActive = millis();
    drawMenu();
}