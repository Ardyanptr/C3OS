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

    if (timerIsRunning()) {
        uint32_t t = timerGetRemain();
        uint8_t m = t / 60;
        uint8_t s = t % 60;

        char buf[6];
        sprintf(buf, "%02d:%02d", m, s);

        display.setFont(u8g2_font_7x14_tf);
        display.drawStr(34, 36, buf);
    } else {
        display.drawStr(28, 36, "AOD");
    }

    display.sendBuffer();
}

inline void startAOD() {
    uint8_t pin = Settings::instance->get().aodPin;
    gpio_num_t wakePin = (gpio_num_t)pin;

    display.setContrast(1);
    displayAOD();

    pinMode(pin, INPUT_PULLUP);

    gpio_wakeup_enable(wakePin, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();

    uint32_t lastTick = millis();
    uint32_t lastRemain = 0xFFFFFFFF;

    delay(200);

    while (true) {
        uint32_t now = millis();

        if (now - lastTick >= 1000) {
            timerTick();
            lastTick = now;
        }

        uint32_t remain = timerGetRemain();
        if (remain != lastRemain) {
            displayAOD();
            lastRemain = remain;
        }

        esp_sleep_enable_timer_wakeup(1000000);

        esp_light_sleep_start();

        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

        if (cause == ESP_SLEEP_WAKEUP_GPIO || digitalRead(pin) == LOW) {
            break;
        }
    }

    gpio_wakeup_disable(wakePin);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    unsigned long releaseStart = millis();
    while (digitalRead(pin) == LOW) {
        appHeartBeat();
        if (millis() - releaseStart > 2000) break;
        delay(10);
    }

    delay(150);
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
        timerTick();

        unsigned long now = millis();
        if (now - lastAnim > 120) {
            phase++;
            lastAnim = now;
        }

        if (now - lockStart > LOCK_TIMEOUT) {
            startAOD();

            now = millis();
            lockStart = now;
            idx = 0;
            curVal = 0;

            display.setContrast(Settings::instance->get().oledContrast);
            draw(false);

            upWasLow = downWasLow = okWasLow = actWasLow = true;
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