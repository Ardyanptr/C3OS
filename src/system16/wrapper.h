#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

#include <vector>

using std::vector;

inline vector<String> wrapText(U8G2& u8g2, String text, int maxWidth) {
    vector<String> lines;
    String line = "", word = "";

    for (char c : text) {
        if (c == ' ' || c == '\n') {
            if (u8g2.getStrWidth((line + word).c_str()) > maxWidth) {
                lines.push_back(line);
                line = word + " ";
            } else {
                line += word + " ";
            }
            word = "";
            if (c == '\n') {
                lines.push_back(line);
                line = "";
            }
        } else
            word += c;
    }

    if (word.length()) line += word;
    if (line.length()) lines.push_back(line);

    return lines;
}