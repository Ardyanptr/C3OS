#ifndef VKEYBOARD_H
#define VKEYBOARD_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <OneButton.h>
#include <esp_task_wdt.h>

class VirtualKeyboard {
public:
    VirtualKeyboard(U8G2* display, OneButton* up, OneButton* down, OneButton* ok);
    String run();

private:
    U8G2*       u8g2;
    OneButton*  btnUp;
    OneButton*  btnDown;
    OneButton*  btnOk;

    // Input state
    String      input      = "";
    bool        done       = false;
    bool        isUpper    = false;
    bool        isSymbol   = false;

    // Cursor position
    int         col        = 0;
    int         row        = 0;
    float       smoothCol  = 0;
    float       smoothRow  = 0;

    // Blinking cursor
    unsigned long lastBlink   = 0;
    bool          cursorVisible = true;

    // Fast repeat for long press
    unsigned long lastRepeat  = 0;
    static constexpr int REPEAT_DELAY_FIRST = 400;
    static constexpr int REPEAT_DELAY_CONT  = 80;
    bool upLongActive   = false;
    bool downLongActive = false;
    int  repeatCount    = 0;

    // Layout
    // 3 modes: lowercase, uppercase, symbol
    // Row 0: numbers
    // Row 1-3: letters / symbols
    // Row 4: [SHIFT/SYM] [SPACE......] [<DEL]
    static constexpr int ROWS     = 5;
    static constexpr int COLS     = 10;

    // lowercase
    const char layoutLower[5][10] = {
        {'1','2','3','4','5','6','7','8','9','0'},
        {'q','w','e','r','t','y','u','i','o','p'},
        {'a','s','d','f','g','h','j','k','l',';'},
        {'z','x','c','v','b','n','m',',','.','?'},
        {'*',' ',' ',' ',' ',' ',' ',' ',' ','<'},
    };

    // uppercase
    const char layoutUpper[5][10] = {
        {'1','2','3','4','5','6','7','8','9','0'},
        {'Q','W','E','R','T','Y','U','I','O','P'},
        {'A','S','D','F','G','H','J','K','L',';'},
        {'Z','X','C','V','B','N','M',',','.','?'},
        {'*',' ',' ',' ',' ',' ',' ',' ',' ','<'},
    };

    // symbol
    const char layoutSymbol[5][10] = {
        {'!','@','#','$','%','^','&','*','(', ')'},
        {'-','_','=','+','[',']','{','}','\\','|'},
        {':',';','\'','"','<','>',',','.','/',':'},
        {'~','`', '@','#','!','?','+','-','=','_'},
        {'*',' ',' ',' ',' ',' ',' ',' ',' ','<'},
    };

    // Layout constants
    static constexpr int KEY_W   = 11;
    static constexpr int KEY_H   = 9;
    static constexpr int START_X = 3;
    static constexpr int START_Y = 22; // below input bar

    // Key dimensions for special keys (row 4)
    // col 0 = SHIFT/SYM (width KEY_W)
    // col 1..8 = SPACE (wide)
    // col 9 = BACKSPACE (width KEY_W)

    // Draw
    void drawKeyboard();
    void drawInputBar();
    void drawKeys();
    void drawSpecialRow();
    char getKey(int r, int c);

    // Actions
    void onUpClick();
    void onDownClick();
    void onOkClick();
    void onUpLong();
    void onDownLong();
    void onOkLong();

    // Static callbacks
    static VirtualKeyboard* instance;
    static void onUpClickStatic();
    static void onDownClickStatic();
    static void onOkClickStatic();
    static void onUpLongStartStatic();
    static void onDownLongStartStatic();
    static void onUpLongStopStatic();
    static void onDownLongStopStatic();
    static void onOkLongStatic();
};

#endif