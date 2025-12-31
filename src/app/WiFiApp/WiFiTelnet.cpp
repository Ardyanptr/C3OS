#include "WiFiTelnet.h"

#include "WiFiTelnetFunction.h"

bool isTelnetRunning = false;

using CmdFn = void (*)(const String& args);

struct Command {
    const char* name;
    CmdFn fn;
};

Command commands[] = {
    {"help", cmd_help},
    {"status", cmd_status},
    {"heap", cmd_heap},
    {"reboot", cmd_reboot},
    {"tinyneo", cmd_neofetch},
    {"uptime", cmd_uptime},
    {"date", cmd_date},
    {"whoami", cmd_whoami},
    {"gpio", cmd_gpio},
};

void handleTelnetCommand(const String& cmdLine) {
    int space = cmdLine.indexOf(' ');
    String name = (space < 0) ? cmdLine : cmdLine.substring(0, space);
    String args = (space < 0) ? "" : cmdLine.substring(space + 1);

    for (auto& c : commands) {
        if (name == c.name) {
            c.fn(args);
            return;
        }
    }

    Serial1.print("to_tnet:Unknown command: ");
    Serial1.println("to_tnet:" + cmdLine);
}

void handleUARTMessagesTelnet() {
    if (Serial1.available()) {
        String msg = Serial1.readStringUntil('\n');
        msg.trim();

        if (msg.startsWith("tnet:")) {
            String userCmd = msg.substring(5);  // Ambil teks setelah "tnet:"
            handleTelnetCommand(userCmd);
        }
    }
}

void runTelnet() {
    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    display.drawLine(0, 12, 126, 12);

    display.setFont(u8g2_font_5x8_tr);
    display.drawStr(2, 22, "Starting...");
    display.drawStr(2, 62, "[OK]: Stop and Back");

    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(1, 9, "Telnet Over ESP");
    display.sendBuffer();

    sendCommand("avr32:telnet-start");
    unsigned long startTime = millis();
    bool confirmed = false;

    while (millis() - startTime < 2000) {
        if (Serial1.available()) {
            String response = Serial1.readStringUntil('\n');
            response.trim();

            if (response == "TELNET_CONFIRMED_OK" || response == "TELNET_ALREADY_ON") {
                confirmed = true;
                break;
            }
        }

        delay(10);
    }

    if (confirmed) {
        display.drawStr(2, 31, "Running telnet...");
        display.sendBuffer();

        isTelnetRunning = true;
    } else {
        display.drawStr(2, 31, "Telnet no response");
        display.sendBuffer();

        isTelnetRunning = false;
    }

    btnOK.attachClick([]() {
        isTelnetRunning = false;
        draw_waitESP8266Close();
        sendCommand("avr32:telnet-stop");
        delay(100);
        force_stop_task();
        drawMenu();
    });

    while (isTelnetRunning) {
        btnOK.tick();
        handleUARTMessagesTelnet();

        yield();
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }

    display.sendBuffer();
}