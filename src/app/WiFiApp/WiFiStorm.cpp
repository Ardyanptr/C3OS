#include "WiFiStorm.h"

#include <Arduino.h>
#include <esp_task_wdt.h>

bool isWiFiStorm_Running = true;

void runWiFiStorm() {
    display.clearBuffer();
    display.sendBuffer();

    isWiFiStorm_Running = true;

    sendCommand("avr32:wifistorm");
    delay(100);

    btnOK.attachClick([]() {
        draw_waitESP8266Close();
        force_stop_task();
        isWiFiStorm_Running = false;
        delay(50);
    });

    while (isWiFiStorm_Running) {
        esp_task_wdt_reset();
        display.setFontMode(1);
        display.setBitmapMode(1);

        display.drawLine(0, 12, 126, 12);

        display.setFont(u8g2_font_6x10_tr);
        display.drawStr(1, 9, "WiFi Storm");

        display.setFont(u8g2_font_5x8_tr);
        display.drawStr(2, 22, "Starting...");
        display.drawStr(2, 62, "[OK]: Stop and Back");

        display.drawStr(2, 31, "In Action!");

        display.sendBuffer();

        btnOK.tick();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    drawMenu();
}