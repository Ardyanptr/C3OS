#include <Arduino.h>
#include <U8g2lib.h>
#include <OneButton.h>
#include <WiFi.h>

#include "component/vkeyboard.h"
#include "component/regis.h"
#include "component/button.h"
#include "component/regis.h"

#include "icons/icon.h"

#include "config/config.h"

extern void drawMenu();

void startWiFiConnect();