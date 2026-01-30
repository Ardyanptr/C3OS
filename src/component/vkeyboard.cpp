#include "vkeyboard.h"

#include <Arduino.h>

VirtualKeyboard* VirtualKeyboard::instance = nullptr;

VirtualKeyboard::VirtualKeyboard(U8G2* display, OneButton* up, OneButton* down, OneButton* ok) {
    u8g2 = display;
    btnUp = up;
    btnDown = down;
    btnOk = ok;
    instance = this;

    btnUp->attachClick(onUpClickStatic);
    btnDown->attachClick(onDownClickStatic);
    btnOk->attachClick(onOkClickStatic);

    btnUp->attachLongPressStart(onUpLongStatic);
    btnDown->attachLongPressStart(onDownLongStatic);
    btnOk->attachLongPressStart(onOkLongStatic);
}

String VirtualKeyboard::run() {
    done = false;
    input = "";
    col = 0;
    row = 0;
    smoothCol = 0;
    smoothRow = 0;

    while (!done) {
        btnUp->tick();
        btnDown->tick();
        btnOk->tick();

        if (btnUp->isLongPressed()) {
            static unsigned long lastMove = 0;
            if (millis() - lastMove > 100) {
                onUpClick();
                lastMove = millis();
            }
        }
        if (btnDown->isLongPressed()) {
            static unsigned long lastMove = 0;
            if (millis() - lastMove > 100) {
                onDownClick();
                lastMove = millis();
            }
        }

        smoothCol += (col - smoothCol) * 0.4;
        smoothRow += (row - smoothRow) * 0.4;

        drawKeyboard();
        delay(10);
    }
    return input;
}

void VirtualKeyboard::drawKeyboard() {
    u8g2->clearBuffer();

    u8g2->setFont(u8g2_font_6x10_tr);
    u8g2->drawBox(0, 0, 128, 10);
    u8g2->setDrawColor(0);
    u8g2->setCursor(2, 8);
    u8g2->print(input);

    u8g2->setCursor(110, 8);
    u8g2->print(isUpper ? "C" : "c");

    u8g2->setDrawColor(1);

    u8g2->setFont(u8g2_font_5x8_tr);
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 10; x++) {
            char key = layout[y][x];

            char displayKey[2];
            if (key == '*') {
                displayKey[0] = isUpper ? '^' : 'v';
            } else {
                displayKey[0] = key;
            }
            displayKey[1] = 0;

            int px = startX + x * (keyW + 1);
            int py = startY + y * (keyH + 2);

            if (x == col && y == row) {
                u8g2->drawBox(px - 1, py - keyH + 1, keyW, keyH);
                u8g2->setDrawColor(0);
                u8g2->drawStr(px, py, displayKey);
                u8g2->setDrawColor(1);
            } else {
                u8g2->drawStr(px, py, displayKey);
            }
        }
    }

    float cursorX = startX + smoothCol * (keyW + 1);
    float cursorY = startY + smoothRow * (keyH + 2);
    u8g2->drawFrame((int)cursorX - 1, (int)cursorY - keyH, keyW + 2, keyH + 2);

    u8g2->sendBuffer();
}

void VirtualKeyboard::onUpClick() {
    row--;
    if (row < 0) row = 3;
}

void VirtualKeyboard::onDownClick() {
    row++;
    if (row > 3) row = 0;
}

void VirtualKeyboard::onOkClick() {
    char k = layout[row][col];
    if (k == '<' && input.length() > 0) {
        input.remove(input.length() - 1);
    } else if (k == '*') {
        isUpper = !isUpper;
    } else if (k != '<' && k != '*' && k != ' ') {
        if (isUpper && k >= 'a' && k <= 'z') {
            input += (char)(k - 32);
        } else {
            input += k;
        }
    }
}

void VirtualKeyboard::onUpLong() {
    col++;
    if (col > 9) col = 0;
}

void VirtualKeyboard::onDownLong() {
    col--;
    if (col < 0) col = 9;
}

void VirtualKeyboard::onOkLong() {
    done = true;
}

void VirtualKeyboard::onUpClickStatic() {
    if (instance) instance->onUpClick();
}
void VirtualKeyboard::onDownClickStatic() {
    if (instance) instance->onDownClick();
}
void VirtualKeyboard::onOkClickStatic() {
    if (instance) instance->onOkClick();
}
void VirtualKeyboard::onUpLongStatic() {
    if (instance) instance->onUpLong();
}
void VirtualKeyboard::onDownLongStatic() {
    if (instance) instance->onDownLong();
}
void VirtualKeyboard::onOkLongStatic() {
    if (instance) instance->onOkLong();
}