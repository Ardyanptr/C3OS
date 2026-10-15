# C3OS Todo

## Selesai
- [x] Audit kode: kode mati, duplikasi, include, hygiene repo
- [x] Bersihkan artefak build dari tracking git (`compile_commands.json`, `pio_build*.txt`)
- [x] Hapus kode mati yang tak pernah direferensikan (lihat "Dihapus" di bawah)
- [x] Rapikan komentar sisa proses di `component/service.h`

## Dihapus (terbukti tak pernah dipanggil)
- `src/system16/security/SecurityUI.cpp` + `.h` — `runSecurityManager()` tak ada di `APP_LIST`
- `src/app/Essential/UXDemo.cpp` + `.h`, `src/component/ui_demo.h`, `src/component/ui_integration.h`
- `src/component/supervisor.cpp` + `.h` — `supervisedRun()` tak pernah dipanggil
- `src/system16/capability.cpp` + `.h` — `applyCapabilityBoundary()` tak pernah dipanggil
- `src/system16/WDTGuard.h` — tak pernah di-include
- `src/include/BLEAppLib.h` — isinya hanya komentar

## Belum dikerjakan (perubahan perilaku / struktur — butuh persetujuan)
- [ ] Sambungkan item menu index 3 "Brightness" dan 15 "Factory Reset" ke handler yang sudah ada (sekarang menunjuk `non_set()`, jadi tombol mati)
- [ ] Satukan `menuItems[]` (`main.cpp:193`) dan `APP_LIST` (`app.cpp:40`) jadi satu `AppDesc` dengan field `label`
- [ ] Tampilkan label ramah pengguna di animasi/notifikasi, bukan nama fungsi (`startScan`)
- [ ] Audit 3 jalur boot (`fast_boot`/`full_boot`/`safe_boot`) — pastikan service yang berbeda antar jalur itu disengaja
- [ ] Gabungkan duplikasi crash handler (`src/UI/` vs `src/system16/`) dan recovery (`src/UI/recovery.h` vs `SystemRescue`)
- [ ] Cabut `#include "all_.h"` dari `boot.h`, `battery_dead.h`, `UI/crash_handler.h`, `config/gameclient.h`
- [ ] Buang `EspSoftwareSerial` dari `lib_deps` (0 pemakaian) dan objek `sensor` VL53L0X (tak pernah dipakai)
- [ ] Tambah env ESP8266 / RP2040 di `platformio.ini`, atau koreksi klaim di README
- [ ] Pindahkan menu drawing & command handler dari `main.cpp` (1.365 baris) ke file terpisah
