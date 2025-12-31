#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include <OneButton.h>

class VirtualKeyboard {
public:
    VirtualKeyboard(U8G2 *display, OneButton *up, OneButton *down, OneButton *ok);
    String run();

private:
    U8G2 *u8g2;
    OneButton *btnUp, *btnDown, *btnOk;

    const char layout[4][10] = {
        {'1','2','3','4','5','6','7','8','9','_'},
        {'q','w','e','r','t','y','u','i','o','p'},
        {'a','s','d','f','g','h','j','k','l','<'},
        {'z','x','c','v','b','n','m',' ','*',' '}
    };

    int col = 0, row = 0;
    String input;
    bool done = false;
    bool isUpper = false;

    int keyW = 10;
    int keyH = 8;
    int startX = 1;
    int startY = 17;

    float smoothCol = 0, smoothRow = 0;

    void drawKeyboard();
    void onUpClick();
    void onDownClick();
    void onOkClick();
    void onUpLong();
    void onDownLong();
    void onOkLong();

    static VirtualKeyboard *instance;
    static void onUpClickStatic();
    static void onDownClickStatic();
    static void onOkClickStatic();
    static void onUpLongStatic();
    static void onDownLongStatic();
    static void onOkLongStatic();
};
