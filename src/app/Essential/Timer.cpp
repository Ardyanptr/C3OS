#include "Timer.h"

#include "esp_task_wdt.h"

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

void timerStop() { timerRunning = false; }

bool timerIsRunning() { return timerRunning; }

uint32_t timerGetRemain() { return timerRemain; }

uint32_t timerGetTotal() { return timerTotal; }

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
    enum State { SETTING, RUNNING, FINISHED };
    State currentState = timerIsRunning() ? RUNNING : SETTING;

    uint32_t targetSeconds = 0;
    float visualProgress = 0.0f;

    // Variabel untuk Animasi Flip
    int offsetM = 0; // Offset vertikal Menit
    int offsetS = 0; // Offset vertikal Detik
    uint8_t lastM = 0, lastS = 0;

    btnOK.attachClick([]() { timerOkPressed = true; });
    btnOK.attachLongPressStart([]() { timerExitPressed = true; });

    while (true) {
        esp_task_wdt_reset();
        appHeartBeat();
        timerTick();
        btnOK.tick();

        unsigned long now = millis();

        if (timerExitPressed) {
            timerExitPressed = false;
            break;
        }

        if (currentState == SETTING) {
            if (digitalRead(BUTTON_UP) == LOW) {
                targetSeconds += 10;
                delay(50);
            }
            if (digitalRead(BUTTON_DOWN) == LOW && targetSeconds >= 10) {
                targetSeconds -= 10;
                delay(50);
            }
            if (timerOkPressed && targetSeconds > 0) {
                timerOkPressed = false;
                timerStart(targetSeconds);
                currentState = RUNNING;
            }
        }
        display.clearBuffer();

        uint32_t displayTime = (currentState == SETTING) ? targetSeconds : timerGetRemain();
        uint8_t m = displayTime / 60;
        uint8_t s = displayTime % 60;

        if (m != lastM) {
            offsetM = 30;
            lastM = m;
        }
        if (s != lastS) {
            offsetS = 30;
            lastS = s;
        }

        if (offsetM > 0) offsetM = (offsetM * 0.7);
        if (offsetS > 0) offsetS = (offsetS * 0.7);

        display.setFont(u8g2_font_logisoso24_tn);
        char buf[3];

        sprintf(buf, "%02d", m);
        display.drawStr(25, 40 + offsetM, buf);

        display.drawStr(60, 38, ":");

        sprintf(buf, "%02d", s);
        display.drawStr(75, 40 + offsetS, buf);

        if (currentState == RUNNING) {
            float targetP = (float)timerGetRemain() / timerGetTotal();
            visualProgress = visualProgress + (targetP - visualProgress) * 0.1f;

            display.setDrawColor(1);
            display.drawBox(10, 50, (int)(108 * visualProgress), 2);
            display.drawBox(10, 50, 2, 4);
            display.drawBox(116, 50, 2, 4);
        }

        if (currentState == SETTING) {
            display.setFont(u8g2_font_4x6_tf);
            display.drawHLine(30, 15, 68);
        }

        display.sendBuffer();
        yield();
        delay(16);
        vTaskDelay(1);
    }
    drawMenu();
}