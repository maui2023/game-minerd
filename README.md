# 🎮⛏️ game-minerd

> **Tukar NerdMiner Anda Menjadi Konsol Retro Mudah Alih Tanpa Mengorbankan Aktiviti Solo Mining di Latar Belakang!**

[![ESP32](https://img.shields.io/badge/Platform-ESP32%20%7C%20ESP32--S3-blue?logo=espressif)](https://www.espressif.com/)
[![FreeRTOS](https://img.shields.io/badge/OS-FreeRTOS%20Dual--Core-orange)](https://www.freertos.org/)
[![Mining](https://img.shields.io/badge/Mining-Bitcoin%20Solo%20Stratum-gold?logo=bitcoin)](https://github.com/BitMaker-hub/NerdMiner_v2)
[![Gaming](https://img.shields.io/badge/Gaming-8--Bit%20Retro%20Handheld-crimson?logo=retroarch)](https://github.com)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

---

## 📖 Pengenalan

**game-minerd** lahir daripada satu idea mudah: *Mengapa NerdMiner hanya dibiarkan berdiri kaku sebagai perhiasan meja pasif apabila mikropengawalnya memiliki kuasa pemprosesan dwi-teras (dual-core) yang mampu melakukan lebih banyak perkara hebat?*

Projek ini mengubah konsep NerdMiner konvensional menjadi sebuah **konsol permainan mudah alih (handheld retro gaming console)**. Anda boleh bermain pelbagai permainan retro klasik (seperti Tetris, Space Invaders, Flappy Bird, Snake, sehingga emulator NES) sambil mikropengawal terus menjalankan operasi **Bitcoin Solo Mining 24/7 di latar belakang** tanpa sebarang gangguan kependaman atau kejatuhan hashrate!

---

## ⚡ Ciri-Ciri Utama (Key Features)

- 🚀 **Operasi Dwi-Teras Sejati (True Dual-Core FreeRTOS Architecture)**:
  - **Core 0**: Dikhaskan 100% untuk operasi perlombongan Bitcoin (SHA-256 hashing, komunikasi Stratum pool, sambungan WiFi).
  - **Core 1**: Dikhaskan untuk enjin paparan grafik, input butang/kawalan permainan, dan audio tanpa mengganggu tugas perlombongan.
- 🕹️ **Konsol Retro Terbina**:
  - Permainan arked terbina (Space Invaders, Flappy Nerd, Tetris, Retro Racer, Pong).
  - Sokongan emulator 8-bit (NES / Game Boy lite) melalui kad MicroSD pada papan yang disokong.
- 📊 **Mining HUD Semasa Bermain**:
  - Mini bar telus (*transparent overlay*) di sudut skrin memaparkan **Hashrate semasa (kH/s / MH/s)**, status sambungan pool, dan rekod kesukaran terbaik (*best diff*).
- 🏆 **Notifikasi Kemenangan Blok (Block Found Alert!)**:
  - Sekiranya blok Bitcoin sah ditemui semasa anda sedang asyik bermain, permainan akan mencetuskan animasi perayaan dan bunyi siren kemenangan khas!
- 💤 **Mod Pintar Screensaver & Papan Pemuka Penuh**:
  - Apabila tiada butang ditekan selama beberapa minit (*idle timeout*), skrin akan beralih secara automatik ke papan pemuka statistik penuh NerdMiner asal (jam, statistik perlombongan, harga BTC).
  - Butang pintas (*Hot-key switch*) untuk menukar mod skrin Game ↔ Mining Dashboard pada bila-bila masa.
- 🌐 **Portal Konfigurasi Web WiFi**:
  - Pengurusan WiFi, alamat dompet Bitcoin (Wallet Address), dan pemilihan kolam solo (cth: Public-Pool, Solo CKPool) melalui mod Access Point (AP).

---

## 🧠 Senibina Sistem (System Architecture)

Mikropengawal ESP32 dibina dengan dua teras pemprosesan fizikal (*Xtensa Dual-Core 32-bit LX6/LX7*). `game-minerd` memanfaatkan kelebihan ini menggunakan pengurusan tugasan **FreeRTOS**:

```
+-------------------------------------------------------------------------+
|                              ESP32 SYSTEM                               |
+------------------------------------+------------------------------------+
|               CORE 0               |               CORE 1               |
|       (Background Solo Mining)     |       (Foreground Gaming Engine)   |
+------------------------------------+------------------------------------+
|  • FreeRTOS Task: MinerTask        |  • FreeRTOS Task: GameLoopTask     |
|  • SHA-256 Micro-Kernel Execution  |  • LovyanGFX / TFT_eSPI Graphics   |
|  • Stratum Protocol Client         |  • Game Input & Button Debounce    |
|  • WiFi Stack & TCP Sockets        |  • Sound / Buzzer Effects          |
|  • Public-Pool / CKPool Ping       |  • Mini Hashrate HUD Rendering     |
+------------------------------------+------------------------------------+
                   \                              /
                    \--- [ Shared Thread-Safe ] -/
                         • Inter-Core Queue (xQueue)
                         • Current Hashrate & Best Diff
                         • Block Discovery Event Signal
```

> **Nota Prestasi:** Oleh kerana tugasan SHA-256 diasingkan pada Core 0, hashrate pelombongan kekal optimum dan stabil walaupun pemain sedang menggerakkan watak atau merender grafik secara intensif pada Core 1!

---

## 🔌 Status Perkakasan Dikesan (Hardware Detected)

Sambungan USB pada peranti yang dipasang telah dikesan seperti berikut:

| Parameter | Maklumat Dikesan |
|---|---|
| **Port Bersiri** | `/dev/ttyUSB0` |
| **Cip USB-to-UART** | **QinHeng Electronics CH340** (`ID 1a86:7523`) |
| **Antara Muka** | CH341-UART Kernel Driver |
| **Model Lazim** | **ESP32 DevKit V1**, **CYD (ESP32-2432S028R)**, atau **LILYGO T-Display (Varian CH340)** |

### 🛠️ Langkah Membuka Akses Port USB di Linux

Sekiranya sistem anda memaparkan `Permission denied` semasa memprogramkan mikropengawal melalui `/dev/ttyUSB0`, laksanakan arahan berikut untuk menambah pengguna ke kumpulan `dialout`:

```bash
sudo usermod -a -G dialout $USER
sudo chmod 666 /dev/ttyUSB0
```
*(Log keluar dan log masuk semula untuk membolehkan perubahan kumpulan berkuat kuasa).*

Untuk memeriksa perincian cip secara terperinci (Chip ID, Flash Size, MAC Address):
```bash
pip install esptool
esptool.py --port /dev/ttyUSB0 chip_id
```

---

## 🎮 Papan Yang Disokong (Supported Hardware Targets)

1. **ESP32-2432S028 ("Cheap Yellow Display" / CYD)**:
   - Skrin sentuh warna 2.8" (320x240 TFT ILI9341).
   - Slot kad MicroSD terbina untuk menyimpan ROM game.
   - Paling ideal untuk konsol game genggam NerdMiner bajet rendah.
2. **LILYGO T-Display / T-Display-S3**:
   - Skrin 1.14" / 1.9" IPS berwarna cerah.
   - 2 butang fizikal terbina untuk kawalan permainan ringkas.
3. **ESP32 DevKit V1 (CH340) + Modul Paparan Luaran**:
   - Modul paparan ST7789 (240x240), ILI9341 (320x240), atau OLED SSD1306 (128x64).
   - Butang punat tekan (tactile switch) atau modul joystick analog.

---

## 🕹️ Permainan: Ular & Tangga (Satoshi Touch Edition)

Permainan ini direka khusus untuk **skrin sentuh (Touchscreen)** papan ESP32 tanpa memerlukan sebarang gamepad fizikal!

### Peraturan & Mekanik Permainan:
1. **Grid Papan (30 Petak)**:
   - Terletak di bahagian kiri skrin (1 hingga 30).
   - Matlamat utama: Capai petak **30 (All-Time High / ATH)** untuk memenangi pusingan!
2. **Token Pemain vs Satoshi Bot**:
   - 🔵 **P1 (Cyan / Anda)**: Token pemain anda.
   - 🔴 **Bot (Merah / Satoshi Bot)**: Bot saingan yang bermain secara automatik.
3. **Tangga (Bull Run Shortcuts 🚀)**:
   - **Petak 3 -> 11**: *Bull Run (+8)*
   - **Petak 8 -> 17**: *Halving Pump (+9)*
   - **Petak 15 -> 26**: *Lightning Network Shortcut (+11)*
   - **Petak 21 -> 29**: *ATH Breakout (+8)*
4. **Ular (Bear Market Dips 📉)**:
   - **Petak 14 -> 4**: *Kena Ular FUD (-10)*
   - **Petak 19 -> 9**: *Bear Market Dip (-10)*
   - **Petak 24 -> 12**: *Crypto Winter (-12)*
   - **Petak 28 -> 16**: *Whale Dump (-12)*
5. **Cara Kawalan Mudah**:
   - Hanya **SENTUH (TAP)** pada butang hijau besar **`TAP DADU 🎲`** di sebelah kanan skrin untuk membaling dadu.
   - *(Pilihan alternatif: Anda juga boleh menekan butang fizikal **BOOT** pada papan untuk membaling dadu).*
6. **Mini Mining HUD & Screensaver**:
   - **Core 0** sentiasa melombong Bitcoin di latar belakang dengan paparan Mini HUD masa nyata di bahagian atas.
   - Jika tiada sentuhan selama 40 saat, skrin beralih secara automatik ke **Mining Dashboard** penuh. Sentuh mana-mana bahagian skrin untuk terus menyambung permainan!

---

## 📦 Pakej Pelepasan & Panduan Flash (Ready-to-Flash Release)

Bagi pengguna yang ingin mem-flash peranti tanpa perlu memasang persekitaran pembangunan PlatformIO atau membuat kompilasi dari kod sumber, gunakan fail binari pelepasan siap pakai di dalam folder [`release/`](release/):

### Pilihan 1: 1-Klik Flash Melalui Pelayar Web (Paling Pantas & Mudah)
*Buka menggunakan Google Chrome / Microsoft Edge / Brave di PC atau Mac:*

1. Sambungkan kabel USB peranti ESP32 CYD ke komputer.
2. Buka salah satu Web Flasher berikut:
   - 👉 [Espressif Web Flasher (esptool-js)](https://espressif.github.io/esptool-js/)
   - 👉 [Adafruit WebSerial ESPTool](https://adafruit.github.io/Adafruit_WebSerial_ESPTool/)
3. Klik **Connect** dan pilih port COM peranti anda.
4. Muat naik fail gabungan **[`release/game-minerd-cyd-2432s028-v1.0.0-merged.bin`](release/game-minerd-cyd-2432s028-v1.0.0-merged.bin)** pada alamat/offset **`0x0`**.
5. Tekan butang **Program / Flash** dan biarkan sehingga 100% selesai!

### Pilihan 2: Skrip Automasi Flash
- **Linux & macOS**:
  ```bash
  cd release
  chmod +x flash.sh
  ./flash.sh /dev/ttyUSB0
  ```
- **Windows**:
  - Dwi-klik pada fail [`release/flash.bat`](release/flash.bat), masukkan nombor port COM anda (cth: `COM3`), dan pilih mod `1`.

---

## 🛠️ Kompilasi Dari Kod Sumber (Build from Source)

### Prasyarat
- [PlatformIO Core (CLI)](https://platformio.org/) atau VS Code bersama Plugin PlatformIO.
- Kabel USB berkualiti yang menyokong pemindahan data.

### Arahan Kompilasi & Flash:
```bash
# 1. Bina & terus flash ke papan CYD yang disambungkan
pio run -e esp32_cyd_2432s028 -t upload

# 2. Atau jana semula pakej pelepasan (merged bin & checksums) secara automatik
./scripts/build_release.sh
```

## 🌐 Pengurusan WiFi & Web Portal (Captive Portal)

Sistem dilengkapi ciri sambungan pintar automatik dan portal web terbina di dalam mikropengawal ESP32:

1. **Sambungan Automatik (NVS Flash Memory)**:
   - Sistem akan mencuba menyambung ke WiFi yang telah dikonfigurasikan (cth: `Kula Diamond`).
2. **Web Config Portal Automatik (Sekiranya WiFi Terputus / Belum Dikonfigurasi)**:
   - Sekiranya sambungan tidak berjaya dalam tempoh 14 saat, peranti akan melancarkan hotspot Access Point (AP) sendiri:
     - **Nama WiFi Hotspot**: `GameMinerd-WiFi`
     - **Kata Laluan Hotspot**: `12345678`
     - **Alamat IP Portal**: `192.168.4.1`
   - Buka pelayar web di telefon pintar atau komputer pada `http://192.168.4.1` untuk:
     - Memasukkan nama WiFi (SSID) & kata laluan baharu.
     - Memasukkan alamat dompet Bitcoin (BTC Wallet).
     - Menukar pelayan mining pool (Stratum).
   - Tekan **Simpan & Sambung Semula** — tetapan disimpan secara kekal di dalam memori flash (NVS) dan peranti akan menyambung ke internet secara automatik!

---

## 🗺️ Pelan Hala Tuju Pembangunan (Roadmap)

- [x] Analisis perkakasan & pengesanan port sambungan USB board (ESP32-CYD CH340 dikesan).
- [x] Rangka kerja dwi-teras FreeRTOS (Core 0: Stratum Miner, Core 1: Render Loop).
- [x] Enjin Midstate SHA-256 bare-metal pantas dengan sokongan Solo (3333) & Pool PPLNS (13333).
- [x] Dual CPU Max Hash (~65-70 kH/s) apabila game berada dalam keadaan idle / screensaver.
- [x] Enjin grafik 60 FPS pantas LovyanGFX dengan sifar flicker (zero flicker).
- [x] Game 1: Papan Permainan Ular & Tangga Interaktif (1 hingga 4 Pemain + Bot Satoshi).
- [x] Game 2: Retro NES 8-Bit Engine dengan On-Screen Overlay Touch Pad (D-Pad Cincin Bulat & Panah Rujukan + Butang A/B Ekstra Besar 42px).
- [x] Sokongan Slot Kad Memori MicroSD CYD (Perkakasan SPI Bebas: CS=5, SCK=18, MISO=19, MOSI=23).
- [x] Pengesanan automatik kad konsol R35S (SDHC/SDXC 128GB) & penyenaraian koleksi Game NES dari direktori `/nes`.
- [x] Game Hub Menu untuk pemilihan permainan secara lancar melalui skrin sentuh.
- [x] Mini HUD hashrate dinamik dan penggera kemenangan blok Bitcoin (*Block Found Alert*).

---

## 🤝 Sumbangan & Komuniti

Sumbangan kod, idea permainan retro baru, dan penambahbaikan grafik amat dialu-alukan! Sila buka *Issue* atau hantarkan *Pull Request*.

## 📄 Lesen

Dilesenkan di bawah [Lesen MIT](LICENSE). Dikuasakan oleh komuniti sumber terbuka NerdMiner dan peminat retro gaming ESP32.
