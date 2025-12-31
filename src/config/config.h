#pragma once
#include <Arduino.h>
#include <OneButton.h>
#include <U8g2lib.h>

#define SDA_PIN 4
#define SCL_PIN 5

#define BUTTON_UP 0
#define BUTTON_DOWN 1
#define BUTTON_OK 2
#define BUTTON_ACTION 3

#define EEPROM_ADDR 0x50
#define EEPROM_SIZE 32668
#define PAGE_SIZE 64
#define INIT_FLAG_ADDR 0

#define OS_VERSION "b0.1.0"
#define ESP32_VERSION "1.0.6"

extern U8G2_SSD1315_128X64_NONAME_F_HW_I2C display;
extern OneButton btnUp;
extern OneButton btnDown;
extern OneButton btnOK;
extern OneButton btnAction;

extern void drawMenu();
extern void enterSleep();

extern unsigned long lastActive;