@echo off
REM ==============================================================================
REM Skrip Flash Pantas game-minerd untuk Windows
REM Papan: ESP32 CYD 2.8" (ESP32-2432S028R ILI9341 + Resistive Touch)
REM ==============================================================================

setlocal enabledelayedexpansion

echo ==========================================================
echo    game-minerd - ESP32 CYD Firmware Flasher (Windows)     
echo ==========================================================
echo.

set /p COM_PORT="Sila masukkan COM Port (cth: COM3): "
if "%COM_PORT%"=="" (
    set COM_PORT=COM3
)

echo Port dipilih: %COM_PORT%
echo.
echo Pilihan Kaedah Flash:
echo 1) Full Flash (Fail Gabungan 0x0 - Bersihkan & Flash Semula Lengkap) [Cadangan]
echo 2) Multi-Binary Flash (0x1000 bootloader, 0x8000 partitions, 0xe000 boot_app0, 0x10000 firmware)
echo 3) App Firmware Sahaja (0x10000 - Kekalkan data WiFi & tetapan)
echo.

set /p CHOICE="Pilih mod [1-3] (Lalai: 1): "
if "%CHOICE%"=="" set CHOICE=1

if "%CHOICE%"=="1" (
    echo.
    echo [*] Memulakan Full Flash (0x0)...
    esptool.py --chip esp32 --port %COM_PORT% --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size 4MB 0x0 game-minerd-cyd-2432s028-v1.0.0-merged.bin
) else if "%CHOICE%"=="2" (
    echo.
    echo [*] Memulakan Multi-Binary Flash...
    esptool.py --chip esp32 --port %COM_PORT% --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size 4MB 0x1000 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
) else if "%CHOICE%"=="3" (
    echo.
    echo [*] Memulakan Flash Firmware Sahaja (0x10000)...
    esptool.py --chip esp32 --port %COM_PORT% --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size 4MB 0x10000 firmware.bin
) else (
    echo [!] Pilihan tidak sah.
    pause
    exit /b 1
)

echo.
echo ==========================================================
echo  [OK] Flash Selesai Berjaya!
echo  Peranti ESP32 CYD anda sedang reboot.
echo ==========================================================
pause
