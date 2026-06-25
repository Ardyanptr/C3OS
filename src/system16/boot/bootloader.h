#pragma once
#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>

enum BootMode { BL_BOOT_NORMAL = 0, BL_BOOT_RECOVERY, BL_BOOT_DFU, BL_BOOT_RCPM, BL_BOOT_COUNT };

class BootloaderUI {
  public:
    BootloaderUI(U8G2 *display, OneButton *btnUp, OneButton *btnDown, OneButton *btnOK);

    BootMode showMenu(uint16_t autoBootSeconds = 3);
    void setSelection(BootMode mode);

  private:
    U8G2 *display;
    OneButton *btnUp;
    OneButton *btnDown;
    OneButton *btnOK;

    // Static instance pointer for callbacks
    static BootloaderUI *instance;

    int selectedIndex;
    float cursorCurrentY;
    float cursorTargetY;
    unsigned long startTime;
    bool menuActive;
    bool selectionMade;

    static const unsigned char arrow_right_bits[];
    void drawStaticUI();
    void drawArrow();
    void drawProgress(float progress);
    void updateUI();

    // Callback wrappers
    static void onUpClick();
    static void onDownClick();
    static void onOkClick();

    // Actual handlers
    void handleUp();
    void handleDown();
    void handleOk();

    BootMode bootModeFromIndex(int idx);
};