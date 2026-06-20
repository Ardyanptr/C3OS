# C3OS - Embedded Operating System

![Platform](https://img.shields.io/badge/Platform-ESP32--C3%20%7C%20ESP8266-blue)
![Language](https://img.shields.io/badge/Language-C%2B%2B%20%2F%20C-00599C)
![Framework](https://img.shields.io/badge/Framework-Arduino-00979D)
![Scripting](https://img.shields.io/badge/Scripting-Lua-2C2D72)
![Status](https://img.shields.io/badge/status-Active%20Development-yellow)

**C3OS** is a lightweight, modular operating system environment built specifically for resource-constrained microcontrollers. The project is written entirely in C++/C using the Arduino Framework, with Lua integrated as an embedded scripting layer for user-level commands and automation. C3OS is designed around three core principles: minimal memory footprint, direct hardware access without abstraction overhead, and a consistent dual-interface experience that works identically whether the user is interacting through a physical OLED display or a remote serial terminal.

The project currently targets two widely available and affordable development boards: the **ESP32-C3 Mini** (RISC-V based) and the **ESP8266 Wemos D1 Mini** (Tensilica Xtensa based). Despite the architectural differences between these two chip families, C3OS is structured so that the same shell logic, command parser, and storage abstraction can run on both platforms with minimal platform-specific code.

C3OS is under active development. The current build already demonstrates solid performance and a stable architectural foundation, but as with any embedded systems project of this scope, occasional bugs and edge cases are expected. The project is updated continuously as new features are implemented, tested, and refined.

---

## Technology Stack

| Layer              | Technology                           | Purpose                                      |
|--------------------|---------------------------------------|-----------------------------------------------|
| Core Firmware      | C++ / C                              | Shell logic, drivers, system core              |
| Build Environment  | Arduino Framework                    | Compilation and board support                  |
| Scripting Layer    | Lua                                   | User-defined commands and automation           |
| Storage            | LittleFS                              | Persistent files, configs, and scripts         |
| Display Driver     | U8g2 (OLED)                           | Local on-device output                         |
| Target Hardware    | ESP32-C3 Mini, ESP8266 Wemos D1 Mini  | Supported boards                               |

---

## Architectural Progression and Design Decisions

During the design phase, three distinct execution paradigms were evaluated before settling on the final architecture. Each approach was tested against the same constraint: the system needed to run application logic and user commands without exceeding a strict 400 KB RAM budget, while still retaining direct access to hardware peripherals.

### 1. External Binary Execution (ELF / BIN Loader)

The first approach explored loading standalone, precompiled C++ binaries (`.bin` or `.elf` files) directly from LittleFS into execution RAM at runtime. While conceptually appealing, this method proved highly impractical on microcontrollers due to the Harvard and execute-in-place (XIP) memory architecture used by both target chips. Executing hardware-dependent functions, such as driving an OLED display or initializing the Wi-Fi stack, from an externally loaded binary would require complex system call hooks or a full symbol translation layer. Without these, the system consistently produced hardware panics, making this approach unsuitable for production use.

### 2. Basic Virtual Machine and Bytecode Interpreters

The second approach considered a higher-level abstraction layer, using either Lua through LuaCompiler or simple Chip8 programs. This method offered strong isolation and stability, since faults in application code would not crash the underlying device. However, both options introduced execution overhead and memory footprints that were difficult to justify given the limited RAM available on the ESP32-C3 and ESP8266, particularly when running alongside display rendering and storage operations.

### 3. Native Embedded Shell Framework (Selected Approach)

The final and currently implemented approach is a direct command-parsing subsystem embedded within the core firmware image (`main.cpp`), tightly coupled to the storage abstraction layer. This design offers maximum execution speed and zero translation overhead, while retaining direct, unrestricted access to native drivers such as U8g2 for display output and LittleFS for persistent storage. This approach was selected as the foundation for C3OS going forward.

---

## System Component Breakdown

The C3OS architecture is structurally divided into three core subsystems that work together to provide a consistent user experience across both supported boards.

**Command Shell and Parser**
Handles input from both the OLED-driven local interface and remote serial terminals, interprets user commands, and dispatches them to the appropriate internal handler. This is also where the Lua scripting layer is invoked for user-defined commands and automation scripts.

**Storage Abstraction Layer**
Built on top of LittleFS, this subsystem manages file read/write operations, configuration persistence, and script storage, exposing a simple and consistent interface to the rest of the system regardless of the underlying flash layout.

**Display and Communication Interface**
Provides a unified output layer that mirrors information across the OLED display (via U8g2) and the serial/remote terminal simultaneously, ensuring that the user receives the same feedback regardless of which interface they are actively using.

---

## Project Status

C3OS is functional and actively maintained. The current implementation reflects a deliberate architectural decision to prioritize speed and direct hardware access over abstraction, and early testing has shown encouraging performance and stability on both target boards. That said, the project is still evolving, and users should expect occasional bugs as new features, commands, and hardware support are added in future updates.

Contributions, issue reports, and feedback are welcome as the project continues to grow.
