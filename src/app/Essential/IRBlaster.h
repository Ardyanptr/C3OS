#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>
#include <WiFi.h>

#include "component/button.h"
#include "component/regis.h"
#include "component/vkeyboard.h"
#include "icons/icon.h"
#include "system16/esp826.h"

extern U8G2_SSD1315_128X64_NONAME_F_HW_I2C display;
extern OneButton btnUp;
extern OneButton btnDown;
extern OneButton btnOK;
extern OneButton btnAction;

extern void drawMenu();

void startIRCopy();