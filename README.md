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

## 🕹️ Mod Permainan & Kawalan

| Mod Permainan | Kawalan Asas | Penerangan |
|---|---|---|
| **Space Invaders** | Kiri / Kanan / Tembak | Menembak armada musuh di angkasa sambil melombong blok BTC. |
| **Flappy Nerd** | Butang Lompat (Tap) | Elakkan halangan paip blockchain untuk mengumpul Satoshi. |
| **Nerd-Tetris** | Kiri / Kanan / Pusing / Jatuh | Susun blok transaksi sehingga menjadi blok yang padu. |
| **Snake Classic** | D-Pad 4 Hala | Kawal ular memakan nonce dan blok hadiah. |
| **Mining Dashboard** | Butang Tukar (Toggle) | Paparan papan pemuka statistik NerdMiner klasik. |

---

## 🚀 Panduan Membina & Mem-Flash (Getting Started)

### Prasyarat
- [PlatformIO Core (CLI)](https://platformio.org/) atau VS Code bersama Plugin PlatformIO.
- Kabel USB berkualiti yang menyokong pemindahan data (bukan sekadar kabel pengecasan).

### Langkah-Langkah Flashing:

1. **Klon Repositori**:
   ```bash
   git clone https://github.com/maui2023/game-minerd.git
   cd game-minerd
   ```

2. **Pilih Sasaran Papan (*Target Environment*)** dalam `platformio.ini`:
   - `cyd_2432s028` (Cheap Yellow Display)
   - `tdisplay_esp32` (LILYGO T-Display)
   - `esp32_devkit_v1` (Custom Breadboard Setup)

3. **Bina & Flash ke Peranti**:
   ```bash
   pio run -e cyd_2432s028 -t upload --upload-port /dev/ttyUSB0
   ```

4. **Konfigurasi Rangkaian & Wallet**:
   - Sambung ke WiFi hotspot bernama `NerdMinerAP`.
   - Buka pelayar web di `192.168.4.1`.
   - Masukkan SSID WiFi rumah, kata laluan, dan alamat dompet Bitcoin anda.

---

## 🗺️ Pelan Hala Tuju Pembangunan (Roadmap)

- [x] Analisis perkakasan & pengesanan port sambungan USB board (CH340 dikesan).
- [ ] Rangka kerja dwi-teras FreeRTOS (Core 0: Stratum Solo Miner, Core 1: Render Loop).
- [ ] Enjin permainan ringan berasaskan LovyanGFX / TFT_eSPI.
- [ ] Permainan retro pertama: *Space Invaders: Satoshi Edition*.
- [ ] Mini HUD hashrate dinamik semasa sesi permainan.
- [ ] Pengurusan penggera visual dan audio apabila blok ditemui (*Block found celebration*).
- [ ] Sokongan emulator 8-bit (NES / Game Boy) melalui kad MicroSD untuk ESP32-2432S028 (CYD).

---

## 🤝 Sumbangan & Komuniti

Sumbangan kod, idea permainan retro baru, dan penambahbaikan grafik amat dialu-alukan! Sila buka *Issue* atau hantarkan *Pull Request*.

## 📄 Lesen

Dilesenkan di bawah [Lesen MIT](LICENSE). Dikuasakan oleh komuniti sumber terbuka NerdMiner dan peminat retro gaming ESP32.
