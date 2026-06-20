#pragma once
#include <Arduino.h>

#define ESP8266_RX 7
#define ESP8266_TX 6

extern TaskHandle_t esp8266TaskHandle;

void setupESP8266Communication();
void sendCommand(String cmd);
void processSerialInput();
void debugESP8266Connection();

void force_stop_task();

bool awaitResponse(const char* response, uint32_t timeoutMs);