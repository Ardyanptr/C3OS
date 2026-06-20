#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include <OneButton.h>

/**
 * @brief Enhanced Recovery Toolset for C3OS
 * Designed to be user-friendly ("Grandparent-tested")
 */
class SystemRescue {
public:
    static void run(U8G2* display, OneButton* up, OneButton* down, OneButton* ok);

private:
    enum RescueTool {
        TOOL_AUTO_FIX,      // Automatic scan and repair
        TOOL_WIPE_CACHE,    // Clear temp files
        TOOL_FACTORY_RESET, // Full wipe
        TOOL_HW_TEST,       // Screen, Buttons, I2C scan
        TOOL_EXIT           // Reboot
    };

    static void showToolMenu();
    static void executeTool(RescueTool tool);
    
    // Recovery Modules
    static void doAutoFix();
    static void doWipeCache();
    static void doFactoryReset();
    static void doHardwareTest();

    static U8G2* _display;
    static OneButton *_up, *_down, *_ok;
    static int _menuIdx;
    static bool _isRunning;
};
