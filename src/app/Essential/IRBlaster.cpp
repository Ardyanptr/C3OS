#include "IRBlaster.h"

void handleIRBlaster() {
    display.drawStr(1, 26, "In Action!");
    display.drawStr(1, 63, "[OK] Exit");
    display.sendBuffer();

    sendCommand("avr32:ir-start");
    delay(50);
    sendCommand("avr32:ir-power");

    btnOK.attachClick([]() {
        sendCommand("avr32:ir-stop");
        force_stop_task();
        delay(100);

        setupESP8266Communication();
        drawMenu();
    });
}

void startIRCopy() {
    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(1, 8, "IR Blaster");

    display.drawLine(0, 9, 127, 9);

    display.setFont(u8g2_font_5x7_tr);
    display.drawStr(1, 18, "Press OK to begin!");
    display.sendBuffer();

    btnOK.attachClick(handleIRBlaster);
}