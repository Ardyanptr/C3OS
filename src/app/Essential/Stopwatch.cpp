#include "Stopwatch.h"

bool onStopwatchRunning = true;

enum stopwatchState { IDLE,
                      RUNNING,
                      PAUSED };

stopwatchState stState = IDLE;

unsigned long startTime = 0;
unsigned long elapsedTime = 0;

void drawStopwatch(unsigned long timeMillis) {
    unsigned int minutes = timeMillis / 60000;
    unsigned int seconds = (timeMillis % 60000) / 1000;
    unsigned int milliseconds = timeMillis % 1000;

    char timeStr[10];
    sprintf(timeStr, "%02u:%02u:%03u", minutes, seconds, milliseconds);

    display.clearBuffer();
    display.setFontMode(1);
    display.setFont(u8g2_font_5x8_tr);
    display.drawStr(42, 23, "Stopwatch");

    display.setFont(u8g2_font_6x13O_tr);
    display.drawStr(37, 39, timeStr);

    display.setFont(u8g2_font_5x7_tr);
    display.drawStr(1, 62, "[OK] Start");
    display.drawStr(77, 62, "[ACT] Exit");

    display.sendBuffer();
}

// Input
void handleOKClick_Stopwatch() {
    startTime = millis();
    elapsedTime = 0;

    stState = RUNNING;
}

void handleActionClick_Stopwatch() {
    startTime = 0;
    elapsedTime = 0;

    stState = IDLE;

    onStopwatchRunning = false;
    drawMenu();
}

// Main Handle
void runStopwatch() {
    stState = IDLE;
    onStopwatchRunning = true;

    startTime = millis();
    elapsedTime = 0;

    while (onStopwatchRunning) {
        appHeartBeat();

        switch (stState) {
            case IDLE:
                drawStopwatch(0);
                btnOK.attachClick(handleOKClick_Stopwatch);
                btnAction.attachClick(handleActionClick_Stopwatch);
                break;
            case RUNNING:
                elapsedTime = millis() - startTime;
                drawStopwatch(elapsedTime);

                btnOK.attachClick([]() {
                    elapsedTime = millis() - startTime;
                    stState = PAUSED;
                });
                break;
            case PAUSED:
                drawStopwatch(elapsedTime);

                btnOK.attachClick([]() {
                    startTime = millis() - elapsedTime;
                    stState = RUNNING;
                });

                break;
        }

        btnOK.tick();
        btnAction.tick();

        yield();
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}