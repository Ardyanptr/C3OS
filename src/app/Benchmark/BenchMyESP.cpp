#include "BenchMyESP.h"

#include <Arduino.h>

#include "esp_task_wdt.h"

// Test
void InjectMemoryFail() {
    while (malloc(256)) {
        appHeartBeat();
    }
}

void DropIPCStress() {
    while (true) {
        appHeartBeat();

        if (esp_random() % 5 == 0) {
            continue;  // silent drop
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void ForceWDTNearTrigger() {
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(9000));
}

// Menu
void showBenchTestMenu() {
    display.clearBuffer();
    display.setFont(u8g2_font_5x7_tr);

    display.drawStr(19, 25, "Inject Memory Fail");
    display.drawStr(27, 35, "Drop IPC Stress");
    display.drawStr(9, 45, "Force WDT Near Trigger");

    display.sendBuffer();

    btnOK.attachClick([]() {
        InjectMemoryFail();
    });

    btnOK.attachDoubleClick([]() {
        DropIPCStress();
    });

    btnOK.attachMultiClick([]() {
        ForceWDTNearTrigger();
    });

    btnOK.attachLongPressStart([]() { drawMenu(); });
}

// Main
void runBenchMyESP() {
    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);
    display.drawXBM(1, 1, 11, 11, image_Untitled_1_bits);

    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(14, 11, "Warning!");

    display.setFont(u8g2_font_5x7_tr);
    display.drawStr(2, 23, "This program will be");
    display.drawStr(2, 31, "crashing your device to");
    display.drawStr(2, 40, "test their stability!");

    display.drawStr(2, 62, "[OK] Continue [OKL] Exit");
    display.sendBuffer();

    btnOK.attachClick(showBenchTestMenu);
    btnOK.attachLongPressStart([]() { drawMenu(); });
}