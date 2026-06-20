#pragma once
#include <Arduino.h>
#include <vector>
#include "system16/app.h"

enum ProcessState {
    PROC_RUNNING,
    PROC_BACKGROUND,
    PROC_FINISHED,
    PROC_KILLED
};

struct Process {
    int id;
    int appIndex;
    const char* name;
    TaskHandle_t handle;
    ProcessState state;
    uint32_t startTime;
    uint32_t lastHeartbeat;
};

class ProcessManager {
public:
    static ProcessManager& instance() {
        static ProcessManager inst;
        return inst;
    }

    int launchApp(int appIndex);
    void killProcess(int id);
    void setForeground(int id);
    int getForegroundId() { return foregroundId; }
    
    std::vector<Process>& getProcesses() { return processes; }
    
    void update();

private:
    ProcessManager() : nextId(1), foregroundId(-1) {}
    
    std::vector<Process> processes;
    int nextId;
    int foregroundId;
};

extern void appHeartBeat();

#include "config/config.h"
