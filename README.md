# C3OS 🖥️

> Lightweight standalone OS for ESP8266 Wemos D1 Mini & ESP32-C3 Super Mini, with OLED SSD1315 and 4-button input

![License](https://img.shields.io/badge/license-MIT-green)
![PlatformIO](https://img.shields.io/badge/platform-PlatformIO-blue)

---

## 🚀 Overview
C3OS adalah **operating system ringan** yang dibuat untuk perangkat **ESP8266 Wemos D1 Mini** dan **ESP32-C3 Super Mini**. Sistem ini berjalan **standalone**, artinya tidak membutuhkan PC atau host lain untuk berfungsi. OS ini dirancang untuk **hobiis, pelajar, dan developer embedded** yang ingin eksplorasi sistem operasi mini, interaksi hardware langsung, serta pengembangan software untuk microcontroller dengan tampilan sederhana.

C3OS menampilkan interface **OLED SSD1315 (128x64)** dan dikendalikan menggunakan **4 tombol**. Desain lightweight-nya membuat OS ini **cepat, hemat memori, dan mudah dikustomisasi**.

---

## 🌍 Target User
C3OS ideal untuk:
- Pelajar / hobiis microcontroller yang ingin belajar OS dasar  
- Developer yang ingin prototipe **embedded standalone system**  
- Orang yang ingin **project portofolio nyata** untuk showcase skill hardware & software  

Pengguna sebaiknya memahami **dasar C/C++ dan PlatformIO**, karena OS ini **terbuka untuk modifikasi** dan debugging langsung di device.

---

## 🏞️ Indoor & Outdoor Use
### Indoor
- Menjalankan **simulasi OS kecil** di meja kerja  
- Menggunakan OS untuk belajar **multi-tasking, driver, dan framebuffer graphics**  
- Bisa jadi **kontrol panel mini** untuk sensor / project IoT lain  

### Outdoor
- Bisa dijadikan **portable device** untuk eksperimen real-world (misal sensor / logging data)  
- Lightweight & standalone → mudah dibawa tanpa kabel / PC  
- OLED + 4 tombol → navigasi langsung tanpa layar besar  

---

## 💾 Installation
```bash
git clone https://github.com/Ardyanptr/C3OS.git
cd C3OS
platformio run
