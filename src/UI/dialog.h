#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

#include "config/config.h"
#include "esp_task_wdt.h"

// Modal confirmation dialog shared by shutdown, restart and their emergency
// variants.
//
// It blocks until the user picks an option and draws its own frames, so the
// caller does not have to keep redrawing around it. Only the OK button is
// borrowed through OneButton; ACTION is polled straight from the GPIO so any
// bindings the caller already installed on it (action menu, "Back", ...) keep
// working the moment the dialog returns.
enum class DialogChoice : uint8_t { CANCEL, CONFIRM, EMERGENCY };

inline DialogChoice confirmDialog(const char* title, const char* line1, const char* line2) {
    static bool sDone = false;
    static DialogChoice sChoice = DialogChoice::CANCEL;

    sDone = false;
    sChoice = DialogChoice::CANCEL;

    btnOK.attachClick([]() {
        sChoice = DialogChoice::CONFIRM;
        sDone = true;
    });
    btnOK.attachLongPressStart([]() {
        sChoice = DialogChoice::EMERGENCY;
        sDone = true;
    });

    const int BOX_X = 14, BOX_Y = 10, BOX_W = 100, BOX_H = 44, R = 5;
    const int TITLE_H = 13;

    while (!sDone) {
        esp_task_wdt_reset();

        if (digitalRead(BUTTON_ACTION) == LOW) {
            while (digitalRead(BUTTON_ACTION) == LOW) {
                esp_task_wdt_reset();
                delay(10);
            }
            sChoice = DialogChoice::CANCEL;
            sDone = true;
        }

        display.clearBuffer();
        display.setFontMode(1);
        display.setBitmapMode(1);
        display.setDrawColor(1);

        display.drawRFrame(BOX_X, BOX_Y, BOX_W, BOX_H, R);
        display.drawRBox(BOX_X, BOX_Y, BOX_W, TITLE_H, R);
        display.drawBox(BOX_X, BOX_Y + TITLE_H - R, BOX_W, R);  // square off the title bar bottom

        // XOR: title text punches through the filled title bar
        display.setDrawColor(2);
        display.setFont(u8g2_font_6x10_tr);
        display.drawStr(BOX_X + (BOX_W - display.getStrWidth(title)) / 2, BOX_Y + 10, title);

        display.setDrawColor(1);
        display.setFont(u8g2_font_4x6_tr);
        if (line1) {
            display.drawStr(BOX_X + (BOX_W - display.getStrWidth(line1)) / 2, BOX_Y + 25, line1);
        }
        if (line2) {
            display.drawStr(BOX_X + (BOX_W - display.getStrWidth(line2)) / 2, BOX_Y + 32, line2);
        }

        const int YES_X = BOX_X + 8, NO_X = BOX_X + 52, BTN_W = 40, BTN_H = 8, BTN_Y = BOX_Y + 34;
        display.drawRFrame(YES_X, BTN_Y, BTN_W, BTN_H, 3);
        display.drawRFrame(NO_X, BTN_Y, BTN_W, BTN_H, 3);

        const char* yesLabel = "OK: Yes";
        const char* noLabel = "ACTION: No";
        display.drawStr(YES_X + (BTN_W - display.getStrWidth(yesLabel)) / 2, BTN_Y + 6, yesLabel);
        display.drawStr(NO_X + (BTN_W - display.getStrWidth(noLabel)) / 2, BTN_Y + 6, noLabel);

        display.sendBuffer();

        btnOK.tick();
        delay(10);
    }

    // Hand the OK button back inert instead of leaving a stale handler armed
    btnOK.attachClick([]() {});
    btnOK.attachLongPressStart([]() {});
    btnOK.tick();

    // Leave the shared display state the way everything else expects it
    display.setDrawColor(1);
    display.setFontMode(0);
    display.setBitmapMode(0);

    esp_task_wdt_reset();
    return sChoice;
}
