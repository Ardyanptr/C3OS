#include <Arduino.h>
#include "WiFiHotspot.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <esp_task_wdt.h>

const byte DNS_PORT = 53;

DNSServer dnsServer;
WebServer server(80);

#include "component/vkeyboard.h"
#include "component/website.h"

VirtualKeyboard *vk;

void hs_onOKLongPress() {
}

void runHotspot() {
    esp_task_wdt_reset();
    vk = new VirtualKeyboard(&display, &btnUp, &btnDown, &btnOK);

    String hs_name = vk->run();

    if(hs_name.length() == 0) {
        display.clearBuffer();
        display.setBitmapMode(1);
        display.drawXBM(0, 22, 45, 42, image_WarningDolphin_bits);

        display.setFont(u8g2_font_t0_11b_tr);
        display.drawStr(50, 47, "Initializing");

        display.setFont(u8g2_font_5x7_tr);
        display.drawStr(57, 60, "WiFi init...");

        display.drawLine(49, 51, 122, 51);
        display.sendBuffer();

        WiFi.mode(WIFI_AP);
        delay(1500);
        WiFi.softAP("Free WiFi", NULL);
        delay(1500);

        IPAddress apIP = WiFi.softAPIP();
        dnsServer.start(DNS_PORT, "*", apIP);

        server.on("/", []() {
            server.send(200, "text/html", portalHTML);
        });

        server.on("/status", []() {
            server.send(200, "application/json", "{\"device\":\"OK\",\"ap\":\"running\"}");
        });

        server.on("/event", []() {
            String action = server.arg("a");
            server.send(200, "text/plain", "RCV:" + action);
        });

        server.onNotFound([]() {
            server.sendHeader("Location", "/", true);
            server.send(302, "text/plain", "");
        });

        server.begin();

        display.clearBuffer();
        display.setFont(u8g2_font_6x10_tf);
        display.drawStr(0, 10, "WiFi SSID Set To:");
        display.drawStr(0, 25, hs_name.c_str());
        display.drawStr(0, 40, "OK to continue");
        display.sendBuffer();

        while(!btnOK.isLongPressed()) {
            btnOK.tick();
            delay(50);
        }
    }

    if(hs_name.length() > 0) {
        display.clearBuffer();
        display.setBitmapMode(1);
        display.drawXBM(0, 22, 45, 42, image_WarningDolphin_bits);

        display.setFont(u8g2_font_t0_11b_tr);
        display.drawStr(50, 47, "Initializing");

        display.setFont(u8g2_font_5x7_tr);
        display.drawStr(57, 60, "WiFi init...");

        display.drawLine(49, 51, 122, 51);
        display.sendBuffer();

        WiFi.mode(WIFI_AP);
        delay(1500);

        WiFi.softAP(hs_name.c_str(), NULL);

        display.clearBuffer();
        display.setFont(u8g2_font_6x10_tf);
        display.drawStr(0, 10, "WiFi SSID Set To:");
        display.drawStr(0, 25, hs_name.c_str());
        display.drawStr(0, 40, "OK to continue");
        display.sendBuffer();

        while(!btnOK.isLongPressed()) {
            btnOK.tick();
            delay(50);
        }
    }

    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(7, 35, hs_name.length() == 0 ? "PRANK AP CREATED" : "AP CREATION SUCCESS");

    display.setFont(u8g2_font_5x7_tr);
    display.drawStr(22, 43, "press OK to close");
    display.sendBuffer();

    display.sendBuffer();

    delete vk;

    detachCallback();

    btnOK.attachClick(drawMenu);
    btnOK.attachLongPressStart(runHotspot);
}