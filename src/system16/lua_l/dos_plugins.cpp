#include "dos_plugins.h"

#include "config/config.h"
#include "system16/lua_l/lua_l.h"

#define DOSPLUGIN_MAX 12

struct DOSPlugin {
    char name[24];
    char desc[48];
    char path[64];
};

static DOSPlugin plugins[DOSPLUGIN_MAX];
static size_t pluginCount = 0;

int DOSPlugins::load() {
    pluginCount = 0;

    File dir = LittleFS.open("/commands");
    if (!dir || !dir.isDirectory()) {
        Serial.println("[DOS/PLUGIN] No /commands/ directory. Create it and drop .lua files in it.");
        return 0;
    }

    File f = dir.openNextFile();
    while (f) {
        if (!f.isDirectory()) {
            String name = f.name();
            if (name.endsWith(".lua") && pluginCount < DOSPLUGIN_MAX) {
                String cmd = name;
                int slash = cmd.lastIndexOf('/');
                if (slash >= 0) cmd = cmd.substring(slash + 1);
                cmd.replace(".lua", "");
                cmd.toLowerCase();
                if (cmd.length() == 0) {
                    f.close();
                    f = dir.openNextFile();
                    continue;
                }

                strncpy(plugins[pluginCount].name, cmd.c_str(), sizeof(plugins[pluginCount].name) - 1);
                plugins[pluginCount].name[sizeof(plugins[pluginCount].name) - 1] = 0;

                String full = name.startsWith("/") ? name : "/" + name;
                strncpy(plugins[pluginCount].path, full.c_str(), sizeof(plugins[pluginCount].path) - 1);
                plugins[pluginCount].path[sizeof(plugins[pluginCount].path) - 1] = 0;

                String firstLine = f.readStringUntil('\n');
                firstLine.trim();
                if (firstLine.startsWith("--")) {
                    String d = firstLine.substring(2);
                    d.trim();
                    if (d.length() > 47) d = d.substring(0, 47);
                    strncpy(plugins[pluginCount].desc, d.c_str(), sizeof(plugins[pluginCount].desc) - 1);
                } else {
                    plugins[pluginCount].desc[0] = 0;
                }

                Serial.printf("[DOS/PLUGIN] Loaded: %s\n", plugins[pluginCount].name);
                pluginCount++;
            }
        }
        f.close();
        f = dir.openNextFile();
    }
    dir.close();

    Serial.printf("[DOS/PLUGIN] %d plugin(s) loaded.\n", pluginCount);
    return pluginCount;
}

bool DOSPlugins::run(const String& cmdline) {
    String word = cmdline;
    int sp = word.indexOf(' ');
    if (sp >= 0) word = word.substring(0, sp);
    word.toLowerCase();
    word.trim();
    if (word.length() == 0) return false;

    for (size_t i = 0; i < pluginCount; i++) {
        if (word.equals(plugins[i].name)) {
            runLuaScriptArgs(plugins[i].path, cmdline);
            return true;
        }
    }
    return false;
}

void DOSPlugins::list() {
    if (pluginCount == 0) {
        Serial.println("No DOS plugins loaded.");
        Serial.println("Drop .lua files into /commands/ and reboot.");
        display.clearBuffer();
        display.drawStr(0, 10, "No plugins found.");
        return;
    }

    Serial.printf("DOS Plugins (%d):\n", pluginCount);
    display.clearBuffer();
    display.drawStr(0, 10, "Plugins:");
    int oledY = 25;
    int shown = 0;

    for (size_t i = 0; i < pluginCount; i++) {
        Serial.printf("  %-12s - %s\n", plugins[i].name,
                      plugins[i].desc[0] ? plugins[i].desc : "no description");
        if (shown < 3) {
            display.setCursor(0, oledY);
            display.print(plugins[i].name);
            oledY += 12;
            shown++;
        }
    }
}

size_t DOSPlugins::count() {
    return pluginCount;
}