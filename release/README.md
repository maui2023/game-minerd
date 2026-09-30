# 📦 game-minerd - Pakej Fail Binari Pelepasan (Release Package v1.0.0)

**Papan Sasaran**: ESP32 CYD 2.8" (ESP32-2432S028R / Cheap Yellow Display - ILI9341 + Resistive Touch)  
**Ciri-Ciri**: Konsol Retro Handheld NES (Zero-Copy 60FPS) + Ular & Tangga Touch Edition + Background Bitcoin Stratum Miner (Solo / Pool PPLNS).

---

## 📂 Kandungan Fail Pelepasan (`release/`)

| Fail | Saiz | Offset Flash | Penerangan |
|---|---|---|---|
| **`game-minerd-cyd-2432s028-v1.0.0-merged.bin`** | **~1.3 MB** | **`0x00000`** | **Fail Gabungan Penuh (All-in-One)**. Sesuai untuk Web Flasher & 1-klik flash. |
| `firmware.bin` | ~1.2 MB | `0x10000` | Kod aplikasi game-minerd sahaja (untuk kemas kini tanpa padam tetapan WiFi). |
| `bootloader.bin` | ~18 KB | `0x01000` | Second stage bootloader ESP32. |
| `partitions.bin` | ~3 KB | `0x08000` | Jadual partition ESP32 (App 1.3MB, SPIFFS 1.4MB). |
| `boot_app0.bin` | 8 KB | `0x0e000` | Penunjuk boot partition OTA ESP32. |
| `flash.sh` | - | - | Skrip automasi flash untuk **Linux & macOS**. |
| `flash.bat` | - | - | Skrip automasi flash untuk **Windows**. |
| `manifest.json` | - | - | Konfigurasi untuk pelayar web (ESP Web Tools). |
| `SHA256SUMS.txt` | - | - | Kod semakan integriti fail (SHA256). |

---

## ⚡ Panduan Cara Flash ke ESP32 CYD

Pilih mana-mana satu kaedah yang paling mudah untuk anda:

---

### Kaedah 1: Menggunakan Pelayar Web (1-Klik Paling Mudah - Tiada Pemasangan Perisian)
*Sesuai untuk Google Chrome, Microsoft Edge, atau Brave pada PC/Mac.*

1. Sambungkan kabel USB peranti ESP32 CYD ke komputer anda.
2. Buka mana-mana Web Flasher berikut:
   - 👉 **Espressif Web Flasher**: [https://espressif.github.io/esptool-js/](https://espressif.github.io/esptool-js/)
   - 👉 **Adafruit Web Flasher**: [https://adafruit.github.io/Adafruit_WebSerial_ESPTool/](https://adafruit.github.io/Adafruit_WebSerial_ESPTool/)
3. Klik **Connect** dan pilih port COM peranti anda (cth: `CP2102` atau `CH340`).
4. Tetapkan fail dan offset:
   - **Fail**: Pilih `game-minerd-cyd-2432s028-v1.0.0-merged.bin`
   - **Offset**: Masukkan `0x0`
   - **Baudrate**: `921600` (atau `460800`)
5. Tekan butang **Program** / **Flash**. Tunggu sehingga 100% siap!

---

### Kaedah 2: Menggunakan Skrip Automasi (`flash.sh` atau `flash.bat`)

#### 🐧 Linux / 🍎 macOS:
Buka Terminal dalam folder ini dan jalankan:
```bash
chmod +x flash.sh
./flash.sh /dev/ttyUSB0
```
*(Gantikan `/dev/ttyUSB0` dengan port peranti anda jika berbeza).*

#### 🪟 Windows:
Dwi-klik pada fail **`flash.bat`**, masukkan nombor port COM anda (cth: `COM3`), dan pilih pilihan `1` untuk Full Flash.

---

### Kaedah 3: Manual Melalui Terminal / Command Prompt (`esptool.py`)

Jika anda mempunyai `esptool.py` terpasang (`pip install esptool`), jalankan satu arahan ini:

#### Pilihan A: Full Flash Fail Gabungan (Cadangan)
```bash
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 \
    --before default_reset --after hard_reset write_flash -z \
    --flash_mode dio --flash_freq 40m --flash_size 4MB \
    0x00000 game-minerd-cyd-2432s028-v1.0.0-merged.bin
```

#### Pilihan B: Multi-Binary Flash (Fail Asal Berasingan)
```bash
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 \
    --before default_reset --after hard_reset write_flash -z \
    --flash_mode dio --flash_freq 40m --flash_size 4MB \
    0x1000 bootloader.bin \
    0x8000 partitions.bin \
    0xe000 boot_app0.bin \
    0x10000 firmware.bin
```

---

## 🕹️ Langkah Pertama Selepas Flash

1. Peranti akan reboot secara automatik. Skrin CYD 2.8" akan menyala memaparkan logo **game-minerd**.
2. **Sambung ke WiFi**:
   - Jika belum disambungkan, sambung ke WiFi Access Point hotspot CYD bernama: `game-minerd-AP` (kata laluan: `12345678`).
   - Buka pelayar di telefon/PC dan layari `http://192.168.4.1` untuk masukkan WiFi rumah anda.
   - Atau jika sudah tersimpan, buka terus IP peranti (cth: `http://192.168.1.48` atau `http://game-minerd.local`).
3. **Mula Bermain**:
   - Buka Web Dashboard, skrol ke **"🎮 Pengurus Katrij NES"**.
   - Tekan **[ ▶ Main di CYD ]** pada *Ice Climber* atau muat naik sebarang game NES kegemaran anda (saiz $\le 64\text{ KB}$).
   - Kawal menggunakan skrin sentuh D-Pad atau butang fizikal **BOOT**!
