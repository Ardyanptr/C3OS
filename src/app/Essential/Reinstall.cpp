#include "Reinstall.h"

#include <vector>
#include <esp_task_wdt.h>
#include <LittleFS.h>

static const char* backupConfig = R"json(
{
  "version": "1.0.0",
  "description": "Critical settings backup configuration for C3OS Security Kernel",
  "auto_backup_enabled": true,
  "backup_interval_ms": 60000,
  "max_backup_count": 5,
  "critical_files": [
    "/settings.bin",
    "/cfg/system_config.json",
    "/cfg/ui_layout.json",
    "/cfg/user_prefs.lua",
    "/cfg/crash_config.json"
  ],
  "recovery_options": {
    "auto_recover_on_corruption": true,
    "format_on_fs_error": true,
    "preserve_user_data": true
  },
  "security_settings": {
    "integrity_check_on_boot": true,
    "watchdog_enabled": true,
    "watchdog_timeout_ms": 10000
  }
}
)json";

static const char* prismrConfig = R"json(
{
    "current_os_version": "/cfg/update.json",
    "check_esp8266": true,
    "auto_sync": true
}
)json";

static const char* systemConfig = R"json(
{
    "boot": {
        "init_tof": true,
        "init_eeprom": true,
        "init_service": true,
        "ignore_hardware": false,
        "no_init": false
    },
    "user_pin": {
        "pin1": 0,
        "pin2": 0,
        "pin3": 0,
        "pin4": 0
    }
}
)json";

static const char* uiLayout = R"json(
{
    "ui": {
        "scroll_duration": 300,
        "visible_items": 3,
        "text_offset_y": 12,
        "draw_count": 4
    },
    "system": {
        "anr_active": false,
        "system_ui_active": true,
        "use_login": true
    }
}
)json";

static const char* updateConfig = R"json(
{
    "ver": "1.1.0010b",
    "wn": "Security Update",
    "file_updated": "/init.lua",
    "updates_file": [
        {
            "local_path": "/security/troops.lua",
            "url": "https://raw.githubusercontent.com/Ardyanptr/C3OS/refs/heads/main/data/security/troops.lua"
        },
        {
            "local_path": "/init.lua",
            "url": "https://raw.githubusercontent.com/Ardyanptr/C3OS/refs/heads/main/data/init.lua"

        },
        {
            "local_path": "/systemd/rclocal.lua",
            "url": "https://raw.githubusercontent.com/Ardyanptr/C3OS/refs/heads/main/data/systemd/rclocal.lua"
        }
    ],
    "updates_config": {
        "local_path": "/cfg/update.json",
        "url": "https://raw.githubusercontent.com/Ardyanptr/C3OS/refs/heads/main/data/cfg/update.json"
    }
}
)json";

const char* userPrefsLua = R"lua(
USER_CONF = {
    theme_color = 1,
    show_fps = true,
    scroll_speed = 1,

    timezone = 7,
    date_format = "DD/MM/YY",

    auto_gc = true,
    debug_mode = false,
}
)lua";

const char* initLua = R"lua(
---@diagnostic disable: undefined-global

_OS_VERSION = "1.1.0b"
print("\n[C3OS-LUA] System Link Initialized")
print("C3OS Version: " .. _OS_VERSION .. "\n")
collectgarbage("incremental", 110, 200)
sys = {
    mem = function() return collectgarbage("count") end,
    gc = function() collectgarbage("generational") end,
    explore = function()
        if runFileManager then
            runFileManager()
        else
            print("Error: FM Not Linked Properly")
        end
    end
}
print("-----------------------")
c3_cls()
c3_update()
c3_drawRFrame(23, 13, 83, 40, 3)
c3_drawBitmap(28, 17, 7, 8, "image_Rpc_active_bits")
c3_set_font(1)
c3_print("Init...", 37, 24)
c3_draw_frame(27, 44, 75, 3)
c3_update()
c3_set_font(4)
c3_print("Prep:", 27, 40)
c3_print("User Data...", 47, 40)
c3_update()
local function load_config()
    local config_path = "/cfg/user_prefs.lua"
    if c3_file_exists(config_path) then
        print("[INIT/CONFIG] Found user preferences!")
    else
        print(
            "[INIT/CONFIG] No user preferences found!")
    end
end
c3_draw_line(28, 45, 65, 45)
c3_update()
local checkFile_ = c3_file_exists("/bin")
if checkFile_ then print("[INIT/BIN] Found bin folder!") end
load_config()
c3_draw_line(28, 45, 100, 45); c3_update()
c3_sleep(100)
print("[LUA/PROC] Initializing Background Process")
local rc_path = "/systemd/rclocal.lua"
print("[LUA/PROC] Finding RC Local...")
if c3_file_exists(rc_path) then
    print("[LUA/RCL] Found RC Local!")
    local code = c3_file_read(rc_path)
    if code then
        print("[RD/LUA/RCL] Executing RC Local...")
        local run_rc, err = load(code)
        if run_rc then
            print("[INIT] Executing RC Local...")
            pcall(run_rc)
        else
            print("[INIT/ERR] Syntax error in rclocal: " .. tostring(err))
        end
    end
else
    print("[INIT/WARN] rclocal.lua not found, skipping services!")
end
sys.gc()
print(string.format("[C3OS-LUA] Initialization Complete (%.2f KB Used)", sys.mem()))
print("-----------------------\n")
)lua";

// Sub Function
void remFileDir(const char* path) {
    File dir = LittleFS.open(path);
    if (!dir || !dir.isDirectory()) return;

    File file = dir.openNextFile();

    while (file) {
        String filePath = file.path();
        file.close();
        
        if(!LittleFS.remove(filePath)) {
            Serial.printf("Failed to remove file: %s\n", filePath.c_str());
        } else {
            Serial.printf("Removed file: %s\n", filePath.c_str());
        }

        file.openNextFile();
    }

    dir.close();
}

// Function
void full_factory_reset() {
    display.clearBuffer();
    display.sendBuffer();

    display.drawXBM(48, 12, 32, 40, image_Updating_bits);
    display.sendBuffer();

    remFileDir("/bin");
    remFileDir("/cfg");
    
    // Remove the init.lua
    File initLua = LittleFS.open("/init.lua");
    if (initLua) {
        initLua.close();
        if(!LittleFS.remove("/init.lua")) {
            Serial.println("Failed to remove /init.lua");
        } else {
            Serial.println("Removed /init.lua");
        }
    } else {
        Serial.println("/init.lua does not exist");
    }

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    display.clearBuffer();
    display.sendBuffer();

    display.drawXBM(48, 7, 32, 40, image_Updating_bits);
    display.setFont(u8g2_font_profont10_tr);
    display.drawStr(27, 58, "Reinstalling...");
    display.sendBuffer();


}

// Main
void runReinstall() {
    while (true) {
        esp_task_wdt_reset();
        appHeartBeat();
        btnOK.tick();

        display.setFont(u8g2_font_profont11_tr);
        display.drawStr(2, 9, "Danger Zone!");
        display.setFont(u8g2_font_profont10_tr);
        display.drawStr(8, 20, "1 > Full Factory Reset");
        display.drawStr(8, 28, "2 > User Settings Reset");
        display.drawStr(8, 60, "3 > Exit");
        display.sendBuffer();

        yield();
        vTaskDelay(1);
    }
}