#include <Arduino.h>
#include "WiFiStorm.h"

void runWiFiStorm() {
    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    display.drawLine(0, 12, 126, 12);

    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(1, 9, "WiFi Storm");

    display.setFont(u8g2_font_5x8_tr);
    display.drawStr(2, 22, "Starting...");
    display.drawStr(2, 62, "[OK]: Stop and Back");

    display.sendBuffer();

    sendCommand("avr32:wifistorm");
    delay(100);

    display.drawStr(2, 31, "In Action!");
    display.sendBuffer();

    btnOK.attachClick([](){
        draw_waitESP8266Close();
        force_stop_task();
        drawMenu();
    });
}