#include "Timer.h"

static bool timerOkPressed = false;
static bool timerExitPressed = false;

static bool timerRunning = false;
static uint32_t timerRemain = 0;
static uint32_t timerTotal = 0;
static unsigned long lastTick = 0;

void timerStart(uint32_t seconds) {
    timerTotal = seconds;
    timerRemain = seconds;
    timerRunning = true;
    lastTick = millis();
}

void timerStop() {
    timerRunning = false;
}

bool timerIsRunning() {
    return timerRunning;
}

uint32_t timerGetRemain() {
    return timerRemain;
}

uint32_t timerGetTotal() {
    return timerTotal;
}

void timerTick() {
    if (!timerRunning) return;

    if (millis() - lastTick >= 1000) {
        lastTick += 1000;
        if (timerRemain > 0)
            timerRemain--;
        else
            timerRunning = false;
    }
}

void runTimerApp() {
    enum State { SETTING,
                 RUNNING,
                 FINISHED };

    State currentState = SETTING;

    uint32_t targetSeconds = 0;
    float visualProgress = 0.0f;

    btnOK.attachClick([]() { timerOkPressed = true; });
    btnOK.attachLongPressStart([]() { timerExitPressed = true; });

    if (timerIsRunning()) {
        targetSeconds = timerGetTotal();
        currentState = RUNNING;
    }

    while (true) {
        appHeartBeat();
        timerTick();

        btnOK.tick();
        btnAction.tick();

        unsigned long now = millis();

        if (timerExitPressed) {
            timerExitPressed = false;
            break;
        }

        if (currentState == SETTING) {
            if (digitalRead(BUTTON_UP) == LOW) {
                if (targetSeconds < 3600) targetSeconds += 10;
                delay(50);
            }
            if (digitalRead(BUTTON_DOWN) == LOW) {
                if (targetSeconds >= 10) targetSeconds -= 10;
                delay(50);
            }
            if (timerOkPressed && targetSeconds > 0) {
                timerOkPressed = false;
                timerStart(targetSeconds);
                currentState = RUNNING;
            }
        } else if (currentState == RUNNING) {
            if (timerOkPressed) {
                timerOkPressed = false;
                timerStop();
                currentState = SETTING;
            }

            if (!timerIsRunning() && timerGetRemain() == 0 && timerGetTotal() > 0) {
                currentState = FINISHED;
            }
        } else if (currentState == FINISHED) {
            if (timerOkPressed) {
                timerOkPressed = false;
                currentState = SETTING;
            }
        }

        display.clearBuffer();

        display.setDrawColor(1);
        display.drawVLine(0, 10, 44);
        display.drawVLine(127, 10, 44);

        display.setFont(u8g2_font_5x8_tr);
        display.drawStr(40, 8, "COUNTDOWN");

        uint32_t displayTime = (currentState == SETTING) ? targetSeconds : timerGetRemain();
        uint8_t m = displayTime / 60;
        uint8_t s = displayTime % 60;

        char timeStr[10];
        sprintf(timeStr, "%02d:%02d", m, s);

        display.setFont(u8g2_font_logisoso32_tn);
        int width = display.getStrWidth(timeStr);
        display.drawStr(64 - (width / 2), 48, timeStr);

        if (currentState == RUNNING && timerIsRunning()) {
            float p = (float)timerGetRemain() / timerGetTotal();
            visualProgress += (p - visualProgress) * 0.15f;

            display.drawRFrame(20, 54, 88, 3, 3);
            display.drawRBox(20, 54, (int)(88 * visualProgress), 3, 3);
        }

        display.setFont(u8g2_font_4x6_tf);
        if (currentState == SETTING) {
            display.drawStr(32, 63, "UP/DN: SET  OK: START");
        } else if (currentState == RUNNING) {
            display.drawStr(42, 63, "OK: STOP/RESET");
        } else {
            if ((now / 500) % 2 == 0) {
                display.setFont(u8g2_font_6x10_tf);
                display.drawStr(40, 58, "FINISHED!");
            }
            display.drawStr(38, 63, "OK: GO BACK");
        }

        display.sendBuffer();
        timerOkPressed = false;
        delay(10);
    }

    btnOK.attachClick(nullptr);
    btnAction.attachClick(nullptr);
    drawMenu();
}