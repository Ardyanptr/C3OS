#include "C3Burner.h"

static bool _exitBench = false;
static void handleExit() { _exitBench = true; }

void runC3Burner() {
    _exitBench = false;
    btnOK.attachClick(handleExit);

    // Setup Sensor Suhu (Legacy Style sesuai requestmu)
    temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
    temp_sensor.dac_offset = TSENS_DAC_L2;
    temp_sensor_set_config(temp_sensor);
    temp_sensor_start();

    float angle = 0;
    float cpuTemp = 0;
    unsigned long lastTempCheck = 0;

    while (!_exitBench) {
        appHeartBeat();
        btnOK.tick();

        uint32_t target = micros() + 20000;  // ~50 FPS lock
        unsigned long startCycle = micros();

        display.clearBuffer();

        // --- MATH INTENSIVE SECTION (3D Torus/Donut) ---
        // Kita hitung 64 titik koordinat 3D setiap frame
        for (float phi = 0; phi < 6.28; phi += 0.4) {
            for (float theta = 0; theta < 6.28; theta += 0.35) {
                volatile float burn = angle;
                for (int i = 0; i < 150; i++) {
                    burn = sin(burn) * cos(burn) + 0.0001;
                }

                float x = (12 + 6 * cos(theta)) * cos(phi + angle);
                float y = (12 + 6 * cos(theta)) * sin(phi + angle);
                float z = 6 * sin(theta);

                // Rotasi Sederhana & Proyeksi
                float xp = 64 + (x * 100) / (100 - z);
                float yp = 32 + (y * 100) / (100 - z);
                display.drawPixel(xp, yp);
            }
        }
        angle += 0.15;

        // --- THERMAL & SYSTEM MONITOR ---
        // Cek suhu setiap 1 detik agar tidak membebani bus sensor
        if (millis() - lastTempCheck > 1000) {
            temp_sensor_read_celsius(&cpuTemp);
            lastTempCheck = millis();

            // Logic Thermal Protection dari kamu
            if (cpuTemp > 60.0) {
                display.setContrast(50);              // Meredupkan OLED
                WiFi.setTxPower(WIFI_POWER_19_5dBm);  // Push WiFi Power
                WiFi.mode(WIFI_STA);
            }
        }

        // --- UI RENDER ---
        display.setFont(u8g2_font_4x6_tf);
        display.setCursor(0, 7);
        display.print("TEMP: ");
        display.print(cpuTemp, 1);
        display.print("C");

        display.setCursor(0, 15);
        display.print("HEAP: ");
        display.print(ESP.getFreeHeap() / 1024);
        display.print("KB");

        unsigned long timeUsed = micros() - startCycle;
        display.setCursor(85, 7);
        display.print("FPS: ");
        display.print(1000000.0 / timeUsed, 1);

        // Stress Indicator Bar
        display.drawFrame(0, 58, 128, 6);
        int loadBar = map(timeUsed, 0, 33000, 0, 126);  // Map ke 30FPS target
        display.drawBox(1, 59, constrain(loadBar, 0, 126), 4);

        display.sendBuffer();
        while ((int32_t)(micros() - target) < 0) {
        }

        if ((millis() & 0x1F) == 0) yield();
    }

    // Cleanup
    temp_sensor_stop();
    btnOK.attachClick(nullptr);
}