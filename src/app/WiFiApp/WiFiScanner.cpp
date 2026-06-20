#include "WiFiScanner.h"

#include <Arduino.h>
#include <WiFi.h>

#include "esp_task_wdt.h"

int WS_selected;
int WS_networkCount = 0;
bool WS_running = false;
bool WS_running_inside = true;

float WS_smoothScroll = 0;
float WS_targetScroll = 0;
float WS_scrollSpeed = 0.25;
int WS_scanProgress = 0;

void WS_drawLiveScan() {
    display.clearBuffer();

    display.setFont(u8g2_font_6x12_tr);
    display.drawStr(0, 10, "SCANNING...");

    display.drawFrame(0, 13, 128, 3);
    int progressWidth = map(WS_scanProgress, 0, 100, 0, 126);
    display.drawBox(1, 14, progressWidth, 1);

    display.setFont(u8g2_font_6x10_tf);
    int count = WiFi.scanComplete();
    if (count > 0) {
        for (int i = 0; i < count && i < 4; i++) {
            display.setCursor(0, 30 + (i * 11));
            String ssid = WiFi.SSID(i);
            if (ssid.length() > 18) ssid = ssid.substring(0, 16) + "..";
            display.print("> " + ssid);
        }
    } else {
        display.drawStr(0, 30, "Searching...");
    }

    display.sendBuffer();
}

void startScan() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.scanDelete();
    delay(100);

    WiFi.scanNetworks(true, true, false, 300);

    WS_scanProgress = 0;
    while (true) {
        esp_task_wdt_reset();
        int status = WiFi.scanComplete();

        if (WS_scanProgress < 99) WS_scanProgress++;

        if (status >= 0) {
            WS_scanProgress = 100;
            WS_networkCount = status;
            WS_drawLiveScan();
            delay(500);
            break;
        }

        WS_drawLiveScan();
        delay(30);
        yield();
        vTaskDelay(1);
    }

    if (WS_networkCount <= 0) {
        display.clearBuffer();
        display.setFont(u8g2_font_6x12_tr);
        display.drawStr(30, 35, "0 NETWORKS");
        display.sendBuffer();
        delay(1500);
        drawMenu();
        return;
    }

    WS_selected = 0;
    WS_smoothScroll = 0;
    WS_running = true;

    detachCallback();
    btnUp.attachClick([]() {
        if (WS_selected > 0) WS_selected--;
    });
    btnDown.attachClick([]() {
        if (WS_selected < WS_networkCount - 1) WS_selected++;
    });
    btnOK.attachClick([]() {
        WS_running_inside = true;
        WS_running = false;
    });
    btnOK.attachLongPressStart([]() {
        WS_running_inside = false;
        WS_running = false;
    });

    while (WS_running) {
        esp_task_wdt_reset();
        yield();

        WS_targetScroll = WS_selected;
        WS_smoothScroll += (WS_targetScroll - WS_smoothScroll) * WS_scrollSpeed;

        display.clearBuffer();
        for (int i = 0; i < WS_networkCount; i++) {
            float yPos = 18 + (i - WS_smoothScroll) * 14;
            int y = (int)yPos;

            if (y > -15 && y < 80) {
                if (i == WS_selected) {
                    display.drawRBox(2, y - 10, 120, 13, 1);
                    display.setDrawColor(0);
                } else {
                    display.setDrawColor(1);
                }

                int rssi = WiFi.RSSI(i);
                int bars = (rssi >= -55)   ? 4
                           : (rssi >= -65) ? 3
                           : (rssi >= -75) ? 2
                           : (rssi >= -85) ? 1
                                           : 0;
                for (int b = 0; b < 4; b++) {
                    if (b < bars)
                        display.drawBox(6 + (b * 3), y - (b * 2) - 1, 2, (b * 2) + 2);
                    else
                        display.drawPixel(6 + (b * 3), y - 1);
                }

                display.setFont(u8g2_font_6x10_tf);
                String ssid = WiFi.SSID(i);
                if (ssid.length() == 0) ssid = "[Hidden]";
                if (ssid.length() > 15) ssid = ssid.substring(0, 13) + "..";
                display.setCursor(24, y);
                display.print(ssid);

                if (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) {
                    display.setFont(u8g2_font_4x6_tr);
                    display.drawStr(112, y - 1, "L");
                }
                display.setDrawColor(1);
            }
        }

        if (WS_networkCount > 4) {
            int barH = map(4, 1, WS_networkCount, 10, 64);
            int barY = map(WS_selected, 0, WS_networkCount - 1, 0, 64 - barH);
            display.drawBox(126, barY, 1, barH);
        }

        display.sendBuffer();
        btnUp.tick();
        btnDown.tick();
        btnOK.tick();
        yield();
        vTaskDelay(1);
    }

    if (WS_running_inside) {
        display.clearBuffer();
        display.setFont(u8g2_font_6x12_tr);
        display.drawStr(0, 10, "NETWORK INFO");
        display.drawHLine(0, 13, 128);
        display.setFont(u8g2_font_6x10_tr);
        display.setCursor(0, 28);
        display.print("SSID: " + WiFi.SSID(WS_selected));
        display.setCursor(0, 40);
        display.print("RSSI: " + String(WiFi.RSSI(WS_selected)) + " dBm");
        display.setCursor(0, 64);
        display.print("CH  : " + String(WiFi.channel(WS_selected)));
        display.sendBuffer();

        btnOK.attachClick([]() { startScan(); });
        btnOK.attachLongPressStart([]() { drawMenu(); });
        while (WS_running_inside) {
            btnOK.tick();
            yield();
            vTaskDelay(1);
        }
    } else {
        drawMenu();
    }

    drawMenu();
}