#include "C3Burner.h"
#include "clib/u8g2.h"
#include "esp_task_wdt.h"
#include <Wire.h>

static bool _exitBench = false;
static void handleExit() { _exitBench = true; }

void runC3Burner() {
    _exitBench = false;

    btnOK.attachClick(handleExit);

    while (!_exitBench) {
        btnOK.tick();
        Wire.requestFrom(0x55, 32);

        char buffer[32] = {0};
        int i = 0;
        while (Wire.available() && i < 31) {
            buffer[i++] = Wire.read();
        }

        uint32_t fps = 0;
        float temp = 0, angle = 0;
        int running = 0;

        sscanf(buffer, "%lu,%f,%f,%d", &fps, &temp, &angle, &running);

        display.clearBuffer();

        if (running) {
            display.setFont(u8g2_font_8x13B_tf);
            display.drawStr(0, 12, "Benchmark");

            display.setFont(u8g2_font_10x20_tf);
            char fpsStr[8];
            sprintf(fpsStr, "%lu", fps);
            display.drawStr(0, 35, fpsStr);

            display.setFont(u8g2_font_6x10_tf);
            display.drawStr(55, 35, "FPS");

            int bar = (int)((angle / 6.28f) * 128);
            display.drawFrame(0, 58, 128, 6);
            display.drawBox(1, 59, constrain(bar, 0, 126), 4);
        } else {
            display.setFont(u8g2_font_8x13B_tf);

            display.drawStr(0, 30, "BENCH STOPPED");
        }

        display.sendBuffer();

        delay(5);
        esp_task_wdt_reset();
        yield();
    }

    btnOK.attachClick(nullptr);
    drawMenu();
}