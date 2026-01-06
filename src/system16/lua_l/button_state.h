#pragma once
#include <Arduino.h>
#include <OneButton.h>

enum BtnEvent {
    BTN_NONE = 0,
    BTN_CLICK,
    BTN_DOUBLE,
    BTN_LONG
};

extern OneButton btnUp;
extern OneButton btnDown;
extern OneButton btnOK;
extern OneButton btnAction;

extern volatile BtnEvent btnUp_Event;
extern volatile BtnEvent btnDown_Event;
extern volatile BtnEvent btnOK_Event;
extern volatile BtnEvent btnAction_Event;