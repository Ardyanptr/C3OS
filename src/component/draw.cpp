#include <Arduino.h>
#include "component/draw.h"

void drawAutoScrollText(const String& text, int startY) {
    static int scrollYOffset = 0;
    static unsigned long lastScrollTime = 0;
    static bool isScrolling = true;

    if(isScrolling && millis() - lastScrollTime > 60) {
        scrollYOffset++;
        lastScrollTime = millis();

        int estimatedHeight = (text.length() / 21 + 3) * 10;
        if (scrollYOffset > estimatedHeight - 50) {
            isScrolling = false;
        }
    }

    display.setFont(u8g2_font_6x10_tf);
    int y = startY - scrollYOffset;
    String currentLine = "";

    for(int i=0;i<text.length();i++) {
        char c = text[i];

        if (c == '\n' || currentLine.length() >= 21) {
            if(y >= -10 && y <= 64) {
                display.drawStr(2, y, currentLine.c_str());
            }

            currentLine = "";
            y += 10;

            if (y > 64 + scrollYOffset) break;
        }

        if (c != '\n') currentLine += c;
    }

    if(currentLine.length() > 0 && y >= -10 && y <= 64) {
        display.drawStr(2, y, currentLine.c_str());
    }
}