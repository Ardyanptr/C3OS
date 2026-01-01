#include "C3Burner.h"

static bool _exitBench = false;
static void handleExit() { _exitBench = true; }

void runC3Burner() {
    _exitBench = false;
    btnOK.attachClick(handleExit);

    // Aktifkan WiFi - Radio adalah sumber panas utama
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
    temp_sensor.dac_offset = TSENS_DAC_L2;
    temp_sensor_set_config(temp_sensor);
    temp_sensor_start();

    float angle = 0;
    float cpuTemp = 0;
    unsigned long lastTempCheck = 0;

    while (!_exitBench) {
        btnOK.tick();

        // --- Pemicu Panas Utama: WiFi Scan Non-stop ---
        // Kita gunakan mode async agar tidak nge-lag di layar
        if (WiFi.scanComplete() != -1) {
            WiFi.scanNetworks(true);  // Mulai scan baru setelah yang lama selesai
        }

        display.clearBuffer();

        // --- ULTRA MATH STRESS ---
        // Tambahkan iterasi lebih banyak (150 -> 500)
        for (float phi = 0; phi < 6.28; phi += 0.5) {
            for (float theta = 0; theta < 6.28; theta += 0.4) {
                volatile float burn = angle;
                // Paksa floating point unit kerja keras
                for (int i = 0; i < 500; i++) {
                    burn = sin(burn) * cos(burn) + 0.0001;
                }

                float x = (12 + 6 * cos(theta)) * cos(phi + angle);
                float y = (12 + 6 * cos(theta)) * sin(phi + angle);
                float z = 6 * sin(theta);
                float xp = 64 + (x * 100) / (100 - z);
                float yp = 32 + (y * 100) / (100 - z);
                display.drawPixel(xp, yp);
            }
        }
        angle += 0.2;

        if (millis() - lastTempCheck > 500) {
            temp_sensor_read_celsius(&cpuTemp);
            lastTempCheck = millis();
        }

        // UI Render
        display.setFont(u8g2_font_4x6_tf);
        display.setCursor(0, 7);
        display.print("TEMP: ");
        display.print(cpuTemp, 1);
        display.print("C");

        // Indikator "HEATING"
        display.setCursor(85, 7);
        display.print(WiFi.scanComplete() == -2 ? "SCANNING..." : "RADIO ON");

        display.drawFrame(0, 58, 128, 6);
        // Bar sekarang menunjukkan suhu relatif ke 80 derajat
        int tempBar = map((int)cpuTemp, 30, 80, 0, 126);
        display.drawBox(1, 59, constrain(tempBar, 0, 126), 4);

        display.sendBuffer();

        // HAPUS TARGET MICROS (Hapus limitasi FPS)
        // Biarkan loop berputar secepat mungkin

        yield();  // Tetap butuh yield agar watchdog tidak reset
    }

    WiFi.mode(WIFI_OFF);
    temp_sensor_stop();
    btnOK.attachClick(nullptr);
}