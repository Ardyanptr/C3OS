#include <Arduino.h>

#include "Browser.h"
#include "UI/lockscreen.h"
#include "WiFiClient.h"
#include "WiFiClientSecure.h"
#include "icons/icon.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_task_wdt.h>
#include <vector>

// ─── Browser Constants ──────────────────────────────────────
static const int BROWSER_LINE_HEIGHT = 11;
static const int MAX_CHARS_PER_LINE  = 21;
static const int VISIBLE_LINES       = 4; // Display area below header
static const int MAX_LINES_STORED    = 150;

// ─── Browser State ──────────────────────────────────────────
struct BrowserState {
    String currentUrl;
    String historyUrl;
    std::vector<String> pageLines;
    std::vector<std::pair<int, String>> links; // Line index -> URL
    int scrollLine = 0;
    int cursorLine = 0;
    bool inBrowser = true;
};

static BrowserState g_state;

// ─── Utility Functions ──────────────────────────────────────

String resolveURL(String current, String target) {
    if (target.startsWith("http")) return target;
    if (target.startsWith("//")) return "https:" + target;

    // Basic domain extraction
    int protEnd = current.indexOf("://");
    if (protEnd == -1) return "https://" + target;
    
    int domainEnd = current.indexOf("/", protEnd + 3);
    String baseDomain = (domainEnd == -1) ? current : current.substring(0, domainEnd);

    if (target.startsWith("/")) {
        return baseDomain + target;
    } else {
        // Relative to current path
        int lastSlash = current.lastIndexOf("/");
        if (lastSlash <= protEnd + 2) return baseDomain + "/" + target;
        return current.substring(0, lastSlash + 1) + target;
    }
}

void showLoading(String msg) {
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tr);
    int w = display.getStrWidth(msg.c_str());
    display.drawStr((128 - w) / 2, 32, msg.c_str());
    display.drawFrame(14, 42, 100, 6);
    display.sendBuffer();
}

void showError(String msg) {
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(10, 25, "Error:");
    display.setFont(u8g2_font_5x7_tr);
    
    String dispMsg = msg;
    if (dispMsg.length() > 25) {
        dispMsg = dispMsg.substring(0, 22) + "...";
    }
    display.drawStr(10, 40, dispMsg.c_str());
    
    display.sendBuffer();
    delay(2000);
}

// ─── HTML Parser ───────────────────────────────────────────

void parseHTML(String &html) {
    g_state.pageLines.clear();
    g_state.links.clear();

    String currentLine = "";
    bool inTag = false;
    bool skipContent = false;
    String currentTag = "";
    String pendingLink = "";
    bool inLink = false;
    int scriptDepth = 0;

    auto flushLine = [&]() {
        if (currentLine.length() > 0) {
            if (inLink && !pendingLink.isEmpty()) {
                g_state.links.push_back({(int)g_state.pageLines.size(), pendingLink});
                currentLine = ">" + currentLine;
            }
            g_state.pageLines.push_back(currentLine);
            currentLine = "";
        }
    };

    for (size_t i = 0; i < html.length() && g_state.pageLines.size() < MAX_LINES_STORED; i++) {
        char c = html[i];

        if (c == '<') {
            inTag = true;
            currentTag = "";
            continue;
        }

        if (c == '>') {
            inTag = false;
            currentTag.toLowerCase();

            if (currentTag.startsWith("script") || currentTag.startsWith("style") || currentTag.startsWith("head") || currentTag.startsWith("nav") || currentTag.startsWith("footer")) {
                skipContent = true;
                scriptDepth++;
            }
            if (currentTag.startsWith("/script") || currentTag.startsWith("/style") || currentTag.startsWith("/head") || currentTag.startsWith("/nav") || currentTag.startsWith("/footer")) {
                scriptDepth = max(0, scriptDepth - 1);
                if (scriptDepth == 0) skipContent = false;
            }

            // Block breaks
            if (currentTag == "p" || currentTag.startsWith("br") || currentTag == "div" || currentTag.startsWith("h") || currentTag == "li" || currentTag == "tr") {
                flushLine();
            }

            // Link extraction
            if (currentTag.startsWith("a ")) {
                inLink = true;
                int hrefPos = currentTag.indexOf("href=");
                if (hrefPos != -1) {
                    char quote = currentTag[hrefPos + 5];
                    int start = hrefPos + 6;
                    int end = currentTag.indexOf(quote, start);
                    if (end > start) pendingLink = currentTag.substring(start, end);
                }
            }
            if (currentTag == "/a") {
                flushLine();
                inLink = false;
                pendingLink = "";
            }
            continue;
        }

        if (inTag) {
            currentTag += c;
        } else if (!skipContent) {
            if (c == '\n' || c == '\r' || c == '\t') c = ' ';
            if (c == ' ' && (currentLine.length() == 0 || currentLine.endsWith(" "))) continue;

            // Basic Entity Decoding
            if (c == '&') {
                if (html.substring(i, i+4) == "&lt;") { c = '<'; i += 3; }
                else if (html.substring(i, i+4) == "&gt;") { c = '>'; i += 3; }
                else if (html.substring(i, i+5) == "&amp;") { c = '&'; i += 4; }
                else if (html.substring(i, i+6) == "&nbsp;") { c = ' '; i += 5; }
            }

            currentLine += c;
            if (currentLine.length() >= MAX_CHARS_PER_LINE) {
                int lastSpace = currentLine.lastIndexOf(' ');
                if (lastSpace > 10) {
                    String nextPart = currentLine.substring(lastSpace + 1);
                    currentLine = currentLine.substring(0, lastSpace);
                    flushLine();
                    currentLine = nextPart;
                } else {
                    flushLine();
                }
            }
        }
    }
    flushLine();

    if (g_state.pageLines.empty()) {
        g_state.pageLines.push_back("No readable content.");
    }
}

// ─── Networking ────────────────────────────────────────────

bool fetchPage(String url) {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000); // Increased timeout for slow redirects

    if (!http.begin(client, url)) return false;
    
    // Set a modern Desktop User-Agent to bypass bot detection and consent loops
    http.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    http.addHeader("Accept-Language", "en-US,en;q=0.9");

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        parseHTML(payload);
        http.end();
        return true;
    }
    
    http.end();
    return false;
}

// ─── UI Rendering ──────────────────────────────────────────

void drawBrowserHeader(String title) {
    display.setDrawColor(1);
    display.drawBox(0, 0, 128, 11);
    display.setDrawColor(0);
    display.setFont(u8g2_font_5x7_tr);
    
    String disp = title;
    if (disp.length() > 24) disp = disp.substring(0, 21) + "...";
    display.drawStr(2, 8, disp.c_str());
    display.setDrawColor(1);
}

void renderBrowser() {
    display.clearBuffer();
    drawBrowserHeader(g_state.currentUrl);

    display.setFont(u8g2_font_6x10_tr);
    int y = 22;

    for (int i = g_state.scrollLine; i < (int)g_state.pageLines.size() && y < 64; i++) {
        bool isCursor = (i == g_state.cursorLine);
        if (isCursor) {
            display.setDrawColor(1);
            display.drawBox(0, y - 9, 128, 11);
            display.setDrawColor(0);
        } else {
            display.setDrawColor(1);
        }

        display.drawStr(2, y, g_state.pageLines[i].c_str());
        y += BROWSER_LINE_HEIGHT;
    }

    // Scrollbar
    if (g_state.pageLines.size() > VISIBLE_LINES) {
        int barH = map(VISIBLE_LINES, 0, g_state.pageLines.size(), 10, 40);
        int barY = map(g_state.scrollLine, 0, g_state.pageLines.size() - VISIBLE_LINES, 12, 64 - barH);
        display.setDrawColor(1);
        display.drawBox(126, barY, 2, barH);
    }

    display.sendBuffer();
}

// ─── App Flow ──────────────────────────────────────────────

void handleViewing() {
    bool viewing = true;
    while (viewing) {
        renderBrowser();
        esp_task_wdt_reset();
        appHeartBeat();

        // Blocking button handling for simple app feel
        if (digitalRead(BUTTON_UP) == LOW) {
            if (g_state.cursorLine > 0) {
                g_state.cursorLine--;
                if (g_state.cursorLine < g_state.scrollLine) g_state.scrollLine = g_state.cursorLine;
            }
            delay(100);
        }
        if (digitalRead(BUTTON_DOWN) == LOW) {
            if (g_state.cursorLine < (int)g_state.pageLines.size() - 1) {
                g_state.cursorLine++;
                if (g_state.cursorLine >= g_state.scrollLine + VISIBLE_LINES) {
                    g_state.scrollLine = g_state.cursorLine - VISIBLE_LINES + 1;
                }
            }
            delay(100);
        }
        if (digitalRead(BUTTON_OK) == LOW) {
            // Check for links
            for (auto &lnk : g_state.links) {
                if (lnk.first == g_state.cursorLine) {
                    g_state.historyUrl = g_state.currentUrl;
                    g_state.currentUrl = resolveURL(g_state.currentUrl, lnk.second);
                    viewing = false; // Trigger reload
                    break;
                }
            }
            delay(200);
        }
        if (digitalRead(BUTTON_ACTION) == LOW) {
            viewing = false; // Go back to menu
            delay(200);
        }
        delay(10);
    }
}

void runBrowser() {
    showLoading("Connecting...");
    WiFiManager wm;
    if (!wm.autoConnect("C3OS-Browser")) {
        showError("WiFi Failed");
        return;
    }

    g_state.inBrowser = true;
    g_state.currentUrl = "";

    while (g_state.inBrowser) {
        // Main Browser Menu
        const char* menu[] = {"Search Google", "Search DuckDuckGo", "Enter URL", "Back to Page", "Exit"};
        int sel = 0;
        bool inMenu = true;

        while (inMenu) {
            display.clearBuffer();
            drawBrowserHeader("C3 MINI BROWSER");
            for (int i = 0; i < 5; i++) {
                int y = 22 + (i * 11);
                if (i == sel) {
                    display.setDrawColor(1);
                    display.drawBox(0, y - 9, 128, 11);
                    display.setDrawColor(0);
                } else {
                    display.setDrawColor(1);
                }
                display.drawStr(10, y, menu[i]);
            }
            display.sendBuffer();

            if (digitalRead(BUTTON_UP) == LOW) { sel = (sel + 4) % 5; delay(150); }
            if (digitalRead(BUTTON_DOWN) == LOW) { sel = (sel + 1) % 5; delay(150); }
            if (digitalRead(BUTTON_OK) == LOW) {
                delay(200);
                VirtualKeyboard vkb(&display, &btnUp, &btnDown, &btnOK);
                
                if (sel == 0) { // Google
                    String q = vkb.run();
                    if (!q.isEmpty()) {
                        q.replace(" ", "+");
                        // &gbv=1 forces Google Basic Version (no JS, lightweight HTML)
                        g_state.currentUrl = "https://www.google.com/search?q=" + q + "&gbv=1";
                        inMenu = false;
                    }
                } else if (sel == 1) { // DDG
                    String q = vkb.run();
                    if (!q.isEmpty()) {
                        q.replace(" ", "+");
                        // DuckDuckGo Lite is perfect for low-power devices
                        g_state.currentUrl = "https://duckduckgo.com/lite/?q=" + q;
                        inMenu = false;
                    }
                } else if (sel == 2) { // URL
                    String u = vkb.run();
                    if (!u.isEmpty()) {
                        if (!u.startsWith("http")) u = "https://" + u;
                        g_state.currentUrl = u;
                        inMenu = false;
                    }
                } else if (sel == 3) { // Back to page
                    if (!g_state.pageLines.empty()) inMenu = false;
                } else if (sel == 4) { // Exit
                    g_state.inBrowser = false;
                    inMenu = false;
                }
            }
            if (digitalRead(BUTTON_ACTION) == LOW) {
                if (!g_state.pageLines.empty()) inMenu = false;
                else { g_state.inBrowser = false; inMenu = false; }
                delay(200);
            }
            esp_task_wdt_reset();
            appHeartBeat();
            delay(10);
        }

        if (!g_state.inBrowser) break;

        // Load and View
        if (!g_state.currentUrl.isEmpty()) {
            showLoading("Loading...");
            if (fetchPage(g_state.currentUrl)) {
                g_state.scrollLine = 0;
                g_state.cursorLine = 0;
                handleViewing();
            } else {
                showError("Load Failed");
                g_state.currentUrl = "";
            }
        }
    }
}