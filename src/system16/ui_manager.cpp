#include <Arduino.h>
#include "ui_manager.h"

void draw_waitESP8266Close() {
    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    display.drawRFrame(30, 24, 70, 17, 3);

    display.drawXBM(34, 29, 7, 7, image_Pin_star_bits);

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(44, 35, "Pending Close");

    display.sendBuffer();
}

void draw_restarting() {
    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    display.drawRFrame(30, 24, 70, 17, 3);

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr(44, 35, "Restarting");

    display.drawXBM(35, 28, 5, 8, image_Attention_bits);

    display.sendBuffer();
}

void draw_restartESP8266() {
    display.setFontMode(1);
    display.setBitmapMode(1);

    display.drawRFrame(25, 9, 78, 45, 3);

    display.drawXBM(29, 13, 10, 8, image_back_btn_bits);
    display.setFont(u8g2_font_5x7_tr);
    display.drawStr(43, 21, "ESP8266");

    display.drawStr(29, 30, "Restarting...");
    display.drawFrame(29, 35, 69, 5);
    display.sendBuffer();

    delay(1000);

    display.drawBox(29, 35, 69, 5);
    display.sendBuffer();
}