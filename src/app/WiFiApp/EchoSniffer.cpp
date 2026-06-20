#include <Arduino.h>
#include "EchoSniffer.h"

String ssids_echosniff[32];
int ssidCount_echosniff = 0;
int cursor_echosniff = 0;

bool ssidSelected_echosniff = false;

void echoSniffHandle_UP() {
    cursor_echosniff = (cursor_echosniff - 1 + ssidCount_echosniff) % ssidCount_echosniff;
}

void echoSniffHandle_DOWN() {
    cursor_echosniff = (cursor_echosniff + 1) % ssidCount_echosniff;
}

void echoSniffHandle_OK() {
    ssidSelected_echosniff = true;
}

void runEchoSniffer() {
    ssidSelected_echosniff = false;
    ssidCount_echosniff = 0;
    cursor_echosniff = 0;

    unsigned long scanStart_echosniff = millis();

    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    display.drawLine(0, 12, 126, 12);
    
    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(1, 9, "Echo Beacon");
    display.sendBuffer();

    sendCommand("avr32-echo-sniff:startscan");

    while(millis() - scanStart_echosniff < 5000) {
        if(Serial1.available()) {
            String line = Serial1.readStringUntil('\n');
            line.trim();

            if(line.startsWith("SSID:")) {
                if(ssidCount_echosniff < 32) ssids_echosniff[ssidCount_echosniff++] = line.substring(5);
            }
        }

        display.clearBuffer();

        display.drawLine(0, 12, 126, 12);
        display.setFont(u8g2_font_6x10_tr);
        display.drawStr(1, 9, "Echo Beacon");

        display.drawStr(2, 22, "Scanning");
        display.drawStr(2, 32, ("Found " + String(ssidCount_echosniff)).c_str());
        display.sendBuffer();
    }

    if(ssidCount_echosniff == 0) {
        display.clearBuffer();
        display.drawLine(0, 12, 126, 12);
    
        display.setFont(u8g2_font_6x10_tr);
        display.drawStr(1, 9, "Echo Beacon");
        display.drawStr(0, 22, "No SSID Found!");
        display.sendBuffer();

        delay(2000);
        return;
    }

    btnUp.attachClick(echoSniffHandle_UP);
    btnDown.attachClick(echoSniffHandle_DOWN);
    btnOK.attachClick(echoSniffHandle_OK);

    while(!ssidSelected_echosniff) {
        display.clearBuffer();
        display.drawStr(0, 10, "Select SSID:");

        for(int i=0;i<ssidCount_echosniff && i < 5;i++) {
            int idx = (cursor_echosniff + i) % ssidCount_echosniff;
            if(i == 0) display.drawStr(0, 25 + i * 10, ("> " + ssids_echosniff[idx]).c_str()); else display.drawStr(10, 25 + i * 10, ssids_echosniff[idx].c_str()); 
        }

        display.sendBuffer();
        
        btnOK.tick(); btnUp.tick(); btnDown.tick();
    }
    
    sendCommand("avr32-echo-sniff:ssid:" + ssids_echosniff[cursor_echosniff]);

    display.clearBuffer();
    display.drawStr(0, 15, "Echoing: ");
    display.drawStr(0, 30, ssids_echosniff[cursor_echosniff].c_str());
    display.sendBuffer();

    btnOK.attachLongPressStart([](){
        draw_waitESP8266Close();
        force_stop_task();
        delay(100);
        runEchoSniffer();
    }); 

    btnOK.attachClick([](){
        draw_waitESP8266Close();
        force_stop_task();
        delay(100);
        drawMenu();
    });
}