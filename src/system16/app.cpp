#include "app.h"

#include <Arduino.h>

extern void startScan();
extern void startWiFiConnect();
extern void runSettings();
extern void runBrowser();
extern void startSniffing();
extern void runBeaconJammerMode();
extern void runPongGame();
extern void runHotspot();
extern void startRulerApp();
extern void run_keyboarcat();
extern void runWiFiStorm();
extern void runEchoSniffer();
extern void startIRCopy();
extern void runFlappyBird();
extern void check_sys_integrity();
extern void runOnlineStory();
extern void runHeapMonitor();
extern void runStopwatch();
extern void runBenchMyESP();
extern void runGameClient_SWT();
extern void runGameClient_SVC();
extern void runGameClient_DOOM();
extern void runC3Burner();
extern void runTelnet();
extern void runFileManager();
extern void runStore();
extern void runTimerApp();
extern void proceedUpdate();
extern void startRadarApp();
extern void runPrismLauncher();
extern void runCrashManager();

extern void non_set();

#define APP_LIST               \
    X(startScan)               \
    X(startWiFiConnect)        \
    X(runSettings)             \
    X(non_set)                 \
    X(runBrowser)              \
    X(startSniffing)           \
    X(runBeaconJammerMode)     \
    X(runPongGame)             \
    X(runHotspot)              \
    X(startRulerApp)           \
    X(run_keyboarcat)          \
    X(runWiFiStorm)            \
    X(runEchoSniffer)          \
    X(startIRCopy)             \
    X(runFlappyBird)           \
    X(non_set)                 \
    X(check_sys_integrity)     \
    X(runOnlineStory)          \
    X(runHeapMonitor)          \
    X(runStopwatch)            \
    X(runBenchMyESP)           \
    X(runGameClient_SWT)       \
    X(runGameClient_SVC)       \
    X(non_set)                 \
    X(runC3Burner)             \
    X(runTelnet)               \
    X(runGameClient_DOOM)      \
    X(runFileManager)          \
    X(runStore)                \
    X(runTimerApp)             \
    X(proceedUpdate)           \
    X(startRadarApp)           \
    X(runPrismLauncher)        \
    X(runCrashManager)

#define X(name) {#name, name, 0, 20 * 1024, true},
AppDesc appTable[] = {APP_LIST};
#undef X

const int APP_COUNT = sizeof(appTable) / sizeof(appTable[0]);
