#pragma once
#include "app/Essential/Timer.h"
#include "config/config.h"
#include "config/var_declare.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"

extern void appHeartBeat();
extern bool systemUIActive;

inline void displayAOD() {
    display.clearBuffer();

    display.setFontMode(1);
    display.setBitmapMode(1);

    display.drawXBM(1, 1, 57, 9, image_wondrlan_bits);

    if (timerIsRunning()) {
        uint32_t t = timerGetRemain();
        uint8_t h = t / 3600;
        uint8_t m = (t % 3600) / 60;
        uint8_t s = t % 60;

        char buf[10];
        sprintf(buf, "%02d:%02d:%02d", h, m, s);

        display.drawRFrame(3, 12, 123, 22, 3);
        display.drawXBM(7, 16, 7, 7, image_Pin_star_bits);
        display.setFont(u8g2_font_5x8_tr);
        display.drawStr(16, 23, "Timer");

        display.drawStr(41, 23, "|");

        display.drawStr(48, 23, buf);

        display.drawLine(6, 28, 122, 28);
        display.drawLine(6, 26, 6, 30);
        display.drawLine(122, 26, 122, 30);

        uint32_t total = timerGetTotal();
        if (total > 0) {
            int progressWidth = (t * 116) / total;
            display.drawBox(6, 27, progressWidth, 3);
        }
    }

    display.sendBuffer();
}

inline void startAOD() {
    setCpuFrequencyMhz(80);
    uint8_t pin = BUTTON_OK;

    pinMode(pin, INPUT_PULLUP);
    digitalWrite(pin, HIGH);

    display.setContrast(1);
    displayAOD();

    uint32_t lastTick = millis();
    uint32_t lastDisplayUpdate = millis();

    while (digitalRead(pin) == LOW) {
        appHeartBeat();
        delay(10);
    }

    while (true) {
        appHeartBeat();
        uint32_t now = millis();

        if (now - lastTick >= 1000) {
            timerTick();
            lastTick += 1000;
        }

        if (now - lastDisplayUpdate >= 1000) {
            displayAOD();
            lastDisplayUpdate = now;
        }

        if (digitalRead(pin) == LOW) {
            break;
        }

        delay(50);
    }

    while (digitalRead(pin) == LOW) {
        delay(10);
    }

    setCpuFrequencyMhz(160);
    display.setContrast(Settings::instance->get().oledContrast);
}

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
    const unsigned long LOCK_TIMEOUT = 10000;

    auto draw = [&](bool flashError) {
        display.clearBuffer();

        display.drawVLine(0, 0, 10);
        display.drawHLine(0, 0, 10);
        display.drawVLine(127, 0, 10);
        display.drawHLine(117, 0, 10);
        display.drawVLine(0, 54, 10);
        display.drawHLine(0, 63, 10);
        display.drawVLine(127, 54, 10);
        display.drawHLine(117, 63, 10);

        display.setFont(u8g2_font_4x6_tf);
        if (flashError) {
            display.setFont(u8g2_font_profont12_tr);
            display.drawStr(28, 16, "Wrong Code!");
        } else {
            display.setFont(u8g2_font_profont12_tr);
            display.drawStr(34, 16, "Enter Code");
        }

        int centerX = 64;
        int spacing = 18;
        int startX = centerX - ((CODE_LEN - 1) * spacing) / 2;

        for (uint8_t i = 0; i < CODE_LEN; ++i) {
            int x = startX + i * spacing;

            if (i < idx) {
                display.drawDisc(x, 42, 3);
            } else if (i == idx) {
                char ch[2] = {(char)('0' + curVal), 0};
                display.setFont(u8g2_font_logisoso16_tn);
                int w = display.getStrWidth(ch);
                display.drawStr(x - (w / 2), 48, ch);

                display.drawHLine(x - 6, 52, 12);
                display.drawHLine(x - 4, 53, 8);
            } else {
                display.drawCircle(x, 42, 2);
            }
        }

        display.sendBuffer();
    };

    bool upWasLow = (digitalRead(BUTTON_UP) == LOW);
    bool downWasLow = (digitalRead(BUTTON_DOWN) == LOW);
    bool okWasLow = (digitalRead(BUTTON_OK) == LOW);
    bool actWasLow = (digitalRead(BUTTON_ACTION) == LOW);

    draw(false);

    while (true) {
        appHeartBeat();
        timerTick();

        unsigned long now = millis();

        if (now - lockStart > LOCK_TIMEOUT) {
            startAOD();

            lockStart = millis();
            idx = 0;
            curVal = 0;

            upWasLow = (digitalRead(BUTTON_UP) == LOW);
            downWasLow = (digitalRead(BUTTON_DOWN) == LOW);
            okWasLow = (digitalRead(BUTTON_OK) == LOW);
            actWasLow = (digitalRead(BUTTON_ACTION) == LOW);
            draw(false);
            continue;
        }

        int upL = digitalRead(BUTTON_UP);
        int dnL = digitalRead(BUTTON_DOWN);
        int okL = digitalRead(BUTTON_OK);
        int acL = digitalRead(BUTTON_ACTION);

        bool changed = false;

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
                    break;
                } else {
                    idx = 0;
                    curVal = 0;
                    draw(true);
                    delay(1000);
                    changed = true;
                }
            } else {
                curVal = 0;
                changed = true;
            }
        }
        okWasLow = (okL == LOW);

        if (changed || (now - lastAnim > 120)) {
            if (now - lastAnim > 120) {
                phase++;
                lastAnim = now;
            }
            draw(false);
        }

        delay(20);
    }

    lastActive = millis();
    delay(100);
    drawMenu();
}