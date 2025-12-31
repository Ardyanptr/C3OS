#include "Browser.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include <vector>

// State Management
static String g_url = "";
static std::vector<String> g_pageLines;
static int g_scrollLine = 0;
static bool g_inBrowser = true;

const int BROWSER_LINE_HEIGHT = 10;
const int MAX_CHARS_PER_LINE = 21;

// --- UI HELPER ---
void drawBrowserHeader(String url) {
    display.setDrawColor(1);
    display.drawBox(0, 0, 128, 11);
    display.setDrawColor(0);
    display.setFont(u8g2_font_5x7_tr);
    if (url.length() > 25) url = url.substring(0, 22) + "...";
    display.drawStr(2, 8, url.c_str());
    display.setDrawColor(1);
}

// --- HTML STRIPPER (Hemat RAM) ---
void parseAndWrap(String& html) {
    g_pageLines.clear();
    String currentLine = "";
    bool inTag = false;
    bool skipContent = false;
    String currentTag = "";

    for (size_t i = 0; i < html.length(); i++) {
        char c = html[i];

        if (c == '<') {
            inTag = true;
            currentTag = "";
            continue;
        }
        if (c == '>') {
            inTag = false;
            currentTag.toLowerCase();
            // Buang konten di dalam tag yang tidak berguna buat tampilan teks
            if (currentTag.startsWith("script") || currentTag.startsWith("style") ||
                currentTag.startsWith("head") || currentTag.startsWith("nav")) {
                skipContent = true;
            }
            if (currentTag == "/script" || currentTag == "/style" ||
                currentTag == "/head" || currentTag == "/nav") {
                skipContent = false;
            }
            // Tambah baris baru jika ketemu tag block
            if (currentTag == "p" || currentTag == "br" || currentTag == "div" || currentTag == "h1") {
                if (currentLine.length() > 0) {
                    g_pageLines.push_back(currentLine);
                    currentLine = "";
                }
            }
            continue;
        }

        if (inTag) {
            currentTag += c;
        } else if (!skipContent) {
            // Hilangkan spasi berlebih dan karakter kontrol
            if (c == '\n' || c == '\r' || c == '\t') c = ' ';
            if (isspace(c) && (currentLine.length() == 0 || currentLine.endsWith(" "))) continue;

            currentLine += c;

            if (currentLine.length() >= MAX_CHARS_PER_LINE) {
                g_pageLines.push_back(currentLine);
                currentLine = "";
            }
        }

        if (g_pageLines.size() > 150) break;  // Limit baris sedikit lebih banyak
    }
    if (currentLine.length() > 0) g_pageLines.push_back(currentLine);
}

// --- FETCH DATA ---
bool loadWebPage(String url) {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setUserAgent("Mozilla/5.0 (ESP32)");

    if (http.begin(client, url)) {
        http.setTimeout(10000);
        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            String payload = http.getString();

            // Limit payload agar RAM tidak meledak (Max 15KB)
            if (payload.length() > 15000) payload = payload.substring(0, 15000);

            parseAndWrap(payload);
            http.end();
            return true;
        }
        http.end();
    }
    return false;
}

// --- RENDER ---
void displayPage() {
    display.clearBuffer();
    drawBrowserHeader(g_url);

    display.setFont(u8g2_font_6x10_tr);
    int y = 22;
    for (int i = g_scrollLine; i < g_pageLines.size() && y < 62; i++) {
        display.drawStr(2, y, g_pageLines[i].c_str());
        y += BROWSER_LINE_HEIGHT;
    }

    // Scrollbar
    if (g_pageLines.size() > 5) {
        int barH = map(5, 0, g_pageLines.size(), 10, 40);
        int barY = map(g_scrollLine, 0, g_pageLines.size(), 12, 64 - barH);
        display.drawBox(125, barY, 2, barH);
    }
    display.sendBuffer();
}

// --- MAIN LOOP BROWSER ---
void runBrowser() {
    WiFiManager wm;

    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(5, 10, "CONNECTING TO WIFI...");
    display.sendBuffer();

    if (!wm.autoConnect("ESP32C3", "123456789")) {
        display.clearBuffer();
        display.setFont(u8g2_font_6x10_tr);
        display.drawStr(5, 10, "FAILED TO CONNECT!");
        display.sendBuffer();
        delay(2000);
    } else {
        display.clearBuffer();
        display.setFont(u8g2_font_6x10_tr);
        display.drawStr(5, 10, "CONNECTED!");
        display.sendBuffer();
        delay(1000);
    }

    g_inBrowser = true;
    while (g_inBrowser) {
        appHeartBeat();

        // MENU UTAMA BROWSER
        int sel = 0;
        bool inHome = true;
        while (inHome) {
            display.clearBuffer();
            display.setDrawColor(1);
            display.drawRBox(0, 18 + (sel * 15), 128, 14, 3);

            display.setFont(u8g2_font_6x10_tr);
            display.setDrawColor(sel == 0 ? 0 : 1);
            display.drawStr(10, 29, "Google Search");
            display.setDrawColor(sel == 1 ? 0 : 1);
            display.drawStr(10, 44, "Enter URL");
            display.setDrawColor(sel == 2 ? 0 : 1);
            display.drawStr(10, 59, "Exit");

            drawBrowserHeader("MINI BROWSER");
            display.sendBuffer();

            if (digitalRead(0) == LOW) {
                sel = (sel + 2) % 3;
                delay(150);
            }
            if (digitalRead(1) == LOW) {
                sel = (sel + 1) % 3;
                delay(150);
            }
            if (digitalRead(2) == LOW) {
                if (sel == 0) {
                    g_url = "https://www.google.com/search?q=esp32";
                    inHome = false;
                }
                if (sel == 1) {
                    VirtualKeyboard* vk = new VirtualKeyboard(&display, &btnUp, &btnDown, &btnOK);
                    g_url = vk->run();
                    delete vk;
                    if (g_url.length() > 0) {
                        if (!g_url.startsWith("http")) g_url = "https://" + g_url;
                        inHome = false;
                    }
                }
                if (sel == 2) {
                    g_inBrowser = false;
                    inHome = false;
                }
            }
        }

        if (!g_inBrowser) break;

        // LOADING
        display.clearBuffer();
        display.drawStr(30, 36, "Loading...");
        display.sendBuffer();

        if (loadWebPage(g_url)) {
            g_scrollLine = 0;
            bool viewing = true;
            while (viewing) {
                displayPage();

                if (digitalRead(0) == LOW) {
                    if (g_scrollLine > 0) g_scrollLine--;
                }
                if (digitalRead(1) == LOW) {
                    if (g_scrollLine < (int)g_pageLines.size() - 4) g_scrollLine++;
                }
                if (digitalRead(2) == LOW) viewing = false;  // Back to home

                // Action Button untuk keluar total
                if (digitalRead(3) == LOW) {
                    viewing = false;
                    g_inBrowser = false;
                }
                delay(10);
            }
        } else {
            display.drawStr(25, 36, "Load Failed!");
            display.sendBuffer();
            delay(1000);
        }
    }
}