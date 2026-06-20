#include "vkeyboard.h"

VirtualKeyboard* VirtualKeyboard::instance = nullptr;

// ─────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────
VirtualKeyboard::VirtualKeyboard(U8G2* display, OneButton* up, OneButton* down, OneButton* ok) {
    u8g2   = display;
    btnUp  = up;
    btnDown = down;
    btnOk  = ok;
    instance = this;

    btnUp->attachClick(onUpClickStatic);
    btnDown->attachClick(onDownClickStatic);
    btnOk->attachClick(onOkClickStatic);

    btnUp->attachLongPressStart(onUpLongStartStatic);
    btnDown->attachLongPressStart(onDownLongStartStatic);
    btnUp->attachLongPressStop(onUpLongStopStatic);
    btnDown->attachLongPressStop(onDownLongStopStatic);
    btnOk->attachLongPressStart(onOkLongStatic);
}

// ─────────────────────────────────────────────────────────
// run() — blocking call, returns typed string
// ─────────────────────────────────────────────────────────
String VirtualKeyboard::run() {
    done       = false;
    input      = "";
    col        = 0;
    row        = 0;
    smoothCol  = 0.0f;
    smoothRow  = 0.0f;
    isUpper    = false;
    isSymbol   = false;
    cursorVisible = true;
    lastBlink  = millis();
    upLongActive   = false;
    downLongActive = false;

    while (!done) {
        btnUp->tick();
        btnDown->tick();
        btnOk->tick();

        esp_task_wdt_reset();

        unsigned long now = millis();

        // Fast repeat for left/right column movement
        if (upLongActive || downLongActive) {
            unsigned long threshold = (repeatCount == 0)
                ? REPEAT_DELAY_FIRST
                : REPEAT_DELAY_CONT;

            if (now - lastRepeat > threshold) {
                if (upLongActive)   onUpClick();
                if (downLongActive) onDownClick();
                lastRepeat = now;
                repeatCount++;
            }
        }

        // Blinking cursor (400ms interval)
        if (now - lastBlink > 400) {
            cursorVisible = !cursorVisible;
            lastBlink = now;
        }

        // Smooth cursor lerp
        smoothCol += (col - smoothCol) * 0.35f;
        smoothRow += (row - smoothRow) * 0.35f;

        drawKeyboard();
        delay(10);
    }

    return input;
}

// ─────────────────────────────────────────────────────────
// getKey — returns key char for current layout
// ─────────────────────────────────────────────────────────
char VirtualKeyboard::getKey(int r, int c) {
    if (isSymbol) return layoutSymbol[r][c];
    if (isUpper)  return layoutUpper[r][c];
    return layoutLower[r][c];
}

// ─────────────────────────────────────────────────────────
// drawKeyboard
// ─────────────────────────────────────────────────────────
void VirtualKeyboard::drawKeyboard() {
    u8g2->clearBuffer();
    drawInputBar();
    drawKeys();
    u8g2->sendBuffer();
}

void VirtualKeyboard::drawInputBar() {
    // Bold border input box
    u8g2->setDrawColor(1);
    u8g2->drawBox(0, 0, 128, 12);

    // Input text (inverted)
    u8g2->setDrawColor(0);
    u8g2->setFont(u8g2_font_6x10_tr);

    // Scroll input text if too long
    String displayInput = input;
    while (displayInput.length() > 0 && (int)(displayInput.length() * 6) > 110) {
        displayInput = displayInput.substring(1);
    }
    u8g2->drawStr(2, 9, displayInput.c_str());

    // Blinking cursor
    if (cursorVisible) {
        int curX = 2 + displayInput.length() * 6;
        if (curX < 120) {
            u8g2->drawBox(curX, 1, 1, 8);
        }
    }

    // Mode indicator (top right)
    u8g2->setFont(u8g2_font_4x6_tr);
    if (isSymbol)     u8g2->drawStr(116, 8, "SYM");
    else if (isUpper) u8g2->drawStr(116, 8, "CAP");
    else              u8g2->drawStr(116, 8, "abc");

    u8g2->setDrawColor(1);

    // Separator line (thick feel — draw 2 lines)
    u8g2->drawHLine(0, 12, 128);
    u8g2->drawHLine(0, 13, 128);
}

void VirtualKeyboard::drawKeys() {
    u8g2->setFont(u8g2_font_5x8_tr);

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {

            // Special row (row 4)
            if (r == 4) continue; // handled separately below

            char key = getKey(r, c);
            char buf[2] = { key, 0 };

            int px = START_X + c * (KEY_W + 1);
            int py = START_Y + r * (KEY_H + 2);

            bool selected = (c == col && r == row);

            if (selected) {
                // Inverted selected key
                u8g2->setDrawColor(1);
                u8g2->drawBox(px - 1, py - KEY_H + 1, KEY_W, KEY_H);
                u8g2->setDrawColor(0);
                u8g2->drawStr(px, py, buf);
                u8g2->setDrawColor(1);
            } else {
                u8g2->drawStr(px, py, buf);
            }
        }
    }

    // ─── Special row (row 4) ────────────────────────────
    // [SHIFT/SYM 11px] [SPACE 84px] [DEL 11px]
    // col 0 = SHIFT/SYM
    // col 1..8 = SPACE
    // col 9 = DEL

    int rowY = START_Y + 4 * (KEY_H + 2);

    // SHIFT/SYM key (col 0)
    {
        bool sel = (col == 0 && row == 4);
        int px = START_X;
        int py = rowY;
        const char* label = isSymbol ? "SYM" : (isUpper ? "CAP" : "shf");
        u8g2->setFont(u8g2_font_4x6_tr);
        if (sel) {
            u8g2->setDrawColor(1);
            u8g2->drawBox(px - 1, py - KEY_H + 1, KEY_W, KEY_H);
            u8g2->setDrawColor(0);
            u8g2->drawStr(px, py - 1, label);
            u8g2->setDrawColor(1);
        } else {
            u8g2->drawFrame(px - 1, py - KEY_H + 1, KEY_W, KEY_H);
            u8g2->drawStr(px, py - 1, label);
        }
    }

    // SPACE key (col 1..8 combined)
    {
        bool sel = (col >= 1 && col <= 8 && row == 4);
        int spaceX = START_X + 1 * (KEY_W + 1);
        int spaceW = 8 * (KEY_W + 1) - 1;
        int py     = rowY;
        u8g2->setFont(u8g2_font_4x6_tr);
        if (sel) {
            u8g2->setDrawColor(1);
            u8g2->drawBox(spaceX - 1, py - KEY_H + 1, spaceW, KEY_H);
            u8g2->setDrawColor(0);
            u8g2->drawStr(spaceX + spaceW / 2 - 6, py - 1, "SPACE");
            u8g2->setDrawColor(1);
        } else {
            u8g2->drawFrame(spaceX - 1, py - KEY_H + 1, spaceW, KEY_H);
            u8g2->drawStr(spaceX + spaceW / 2 - 6, py - 1, "SPACE");
        }
    }

    // DEL key (col 9)
    {
        bool sel = (col == 9 && row == 4);
        int px = START_X + 9 * (KEY_W + 1);
        int py = rowY;
        u8g2->setFont(u8g2_font_4x6_tr);
        if (sel) {
            u8g2->setDrawColor(1);
            u8g2->drawBox(px - 1, py - KEY_H + 1, KEY_W, KEY_H);
            u8g2->setDrawColor(0);
            u8g2->drawStr(px, py - 1, "DEL");
            u8g2->setDrawColor(1);
        } else {
            u8g2->drawFrame(px - 1, py - KEY_H + 1, KEY_W, KEY_H);
            u8g2->drawStr(px, py - 1, "DEL");
        }
    }

    // Smooth cursor frame (thick — draw twice offset by 1)
    {
        int sRow = row;
        int sCol = col;

        float cursorX, cursorY;
        int   cursorW;

        if (sRow == 4) {
            if (sCol == 0) {
                cursorX = START_X - 1;
                cursorY = START_Y + 4 * (KEY_H + 2) - KEY_H;
                cursorW = KEY_W + 2;
            } else if (sCol == 9) {
                cursorX = START_X + 9 * (KEY_W + 1) - 1;
                cursorY = START_Y + 4 * (KEY_H + 2) - KEY_H;
                cursorW = KEY_W + 2;
            } else {
                // space bar
                int spaceX = START_X + 1 * (KEY_W + 1);
                int spaceW = 8 * (KEY_W + 1) - 1;
                cursorX = spaceX - 2;
                cursorY = START_Y + 4 * (KEY_H + 2) - KEY_H;
                cursorW = spaceW + 2;
            }
        } else {
            cursorX = START_X + smoothCol * (KEY_W + 1) - 2;
            cursorY = START_Y + smoothRow * (KEY_H + 2) - KEY_H;
            cursorW = KEY_W + 3;
        }

        // Thick frame = draw frame + inner frame
        u8g2->setDrawColor(1);
        u8g2->drawFrame((int)cursorX,     (int)cursorY,     cursorW,     KEY_H + 2);
        u8g2->drawFrame((int)cursorX - 1, (int)cursorY - 1, cursorW + 2, KEY_H + 4);
    }
}

// ─────────────────────────────────────────────────────────
// Actions
// ─────────────────────────────────────────────────────────
void VirtualKeyboard::onUpClick() {
    // Up = move col LEFT
    col--;
    if (col < 0) col = COLS - 1;
}

void VirtualKeyboard::onDownClick() {
    // Down = move col RIGHT
    col++;
    if (col > COLS - 1) col = 0;
}

void VirtualKeyboard::onOkClick() {
    if (row == 4) {
        // Special row
        if (col == 0) {
            // SHIFT/SYM cycling: abc -> CAP -> SYM -> abc
            if (!isUpper && !isSymbol)  { isUpper = true;  isSymbol = false; }
            else if (isUpper)            { isUpper = false; isSymbol = true;  }
            else                         { isUpper = false; isSymbol = false; }
        } else if (col == 9) {
            // DEL
            if (input.length() > 0) input.remove(input.length() - 1);
        } else {
            // SPACE
            input += ' ';
        }
        return;
    }

    char k = getKey(row, col);
    if (k == '<') {
        if (input.length() > 0) input.remove(input.length() - 1);
    } else if (k == '*') {
        if (!isUpper && !isSymbol)  { isUpper = true;  isSymbol = false; }
        else if (isUpper)            { isUpper = false; isSymbol = true;  }
        else                         { isUpper = false; isSymbol = false; }
    } else {
        input += k;
    }

    // Reset blink on keypress — gives visual feedback
    cursorVisible = true;
    lastBlink = millis();
}

void VirtualKeyboard::onUpLong() {
    // Long Up = move row UP
    row--;
    if (row < 0) row = ROWS - 1;
}

void VirtualKeyboard::onDownLong() {
    // Long Down = move row DOWN
    row++;
    if (row > ROWS - 1) row = 0;
}

void VirtualKeyboard::onOkLong() {
    done = true;
}

// ─────────────────────────────────────────────────────────
// Long press fast repeat management
// ─────────────────────────────────────────────────────────
void VirtualKeyboard::onUpLongStartStatic() {
    if (!instance) return;
    instance->upLongActive = true;
    instance->repeatCount  = 0;
    instance->lastRepeat   = millis();
    // Immediately do first row move
    instance->onUpLong();
}

void VirtualKeyboard::onDownLongStartStatic() {
    if (!instance) return;
    instance->downLongActive = true;
    instance->repeatCount    = 0;
    instance->lastRepeat     = millis();
    instance->onDownLong();
}

void VirtualKeyboard::onUpLongStopStatic() {
    if (instance) instance->upLongActive = false;
}

void VirtualKeyboard::onDownLongStopStatic() {
    if (instance) instance->downLongActive = false;
}

// ─────────────────────────────────────────────────────────
// Static callbacks
// ─────────────────────────────────────────────────────────
void VirtualKeyboard::onUpClickStatic()   { if (instance) instance->onUpClick(); }
void VirtualKeyboard::onDownClickStatic() { if (instance) instance->onDownClick(); }
void VirtualKeyboard::onOkClickStatic()   { if (instance) instance->onOkClick(); }
void VirtualKeyboard::onOkLongStatic()    { if (instance) instance->onOkLong(); }