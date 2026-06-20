#pragma once
#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>

#include "component/ui_enhancements.h"
#include "icons/icon.h"

#include "config/config.h"

extern void drawMenu();

namespace UI {

using AlertCallback = void (*)();

struct Alert {
    char title[20];
    char col1[22];
    char col2[22];
    char col3[22];
    AlertCallback callback = nullptr;
};

inline Alert currentAlert;

// MAX TITLE: 19 CHAR, MAX MESSAGE: 63 CHAR (21 NEWLINE)
void showAlert(const char *title, const char *message_col1, const char *message_col2,
               const char *message_col3, AlertCallback cb = nullptr) {
    strncpy(currentAlert.title, title, sizeof(currentAlert.title) - 1);
    currentAlert.title[sizeof(currentAlert.title) - 1] = 0;

    strncpy(currentAlert.col1, message_col1, sizeof(currentAlert.col1) - 1);
    currentAlert.col1[sizeof(currentAlert.col1) - 1] = 0;

    strncpy(currentAlert.col2, message_col2, sizeof(currentAlert.col2) - 1);
    currentAlert.col2[sizeof(currentAlert.col2) - 1] = 0;

    strncpy(currentAlert.col3, message_col3, sizeof(currentAlert.col3) - 1);
    currentAlert.col3[sizeof(currentAlert.col3) - 1] = 0;

    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    // Block / Window
    display.drawRFrame(18, 9, 96, 49, 4);
    display.drawRBox(17, 8, 95, 48, 4);

    display.setDrawColor(2);
    display.setFont(u8g2_font_4x6_tr);

    // Title
    display.drawXBM(21, 12, 7, 7, image_ButtonCenter_bits);
    display.drawStr(30, 18, currentAlert.title);

    // Button
    display.drawRFrame(84, 45, 25, 9, 3);
    display.drawRFrame(58, 45, 25, 9, 3);

    display.drawStr(67, 52, "OK");
    display.drawStr(95, 52, "C");

    // Messages
    display.drawStr(21, 27, currentAlert.col1);
    display.drawStr(21, 34, currentAlert.col2);
    display.drawStr(21, 41, currentAlert.col3);

    display.sendBuffer();

    // Add enhanced UI feedback during alert
    UX::ButtonFeedback::update();
    UX::MemoryMonitor::drawMemoryBar();
    display.sendBuffer();

    btnOK.attachClick([]() {
        if (currentAlert.callback) {
            currentAlert.callback();
            currentAlert.callback = nullptr;
        }

        return;
    });

    btnOK.attachDoubleClick([]() {
        drawMenu();
        return;
    });
}

} // namespace UI