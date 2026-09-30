#!/bin/bash
# ==============================================================================
# Skrip Flash Pantas game-minerd untuk Linux & macOS
# Papan: ESP32 CYD 2.8" (ESP32-2432S028R ILI9341 + Resistive Touch)
# ==============================================================================

set -e

PORT="${1:-/dev/ttyUSB0}"
BAUD="921600"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=========================================================="
echo "   game-minerd - ESP32 CYD Firmware Flasher               "
echo "=========================================================="
echo "Port: ${PORT}"
echo "Baud: ${BAUD}"
echo ""

# Semak sama ada esptool terpasang
ESPTOOL=""
if command -v esptool.py >/dev/null 2>&1; then
    ESPTOOL="esptool.py"
elif command -v esptool >/dev/null 2>&1; then
    ESPTOOL="esptool"
elif [ -f ~/.platformio/packages/tool-esptoolpy/esptool.py ]; then
    ESPTOOL="python3 ~/.platformio/packages/tool-esptoolpy/esptool.py"
else
    echo "[!] esptool tidak ditemui."
    echo "    Sila pasang menggunakan: pip install esptool"
    exit 1
fi

echo "Pilihan Kaedah Flash:"
echo "1) Full Flash (Fail Gabungan 0x0 - Bersihkan & Flash Semula Lengkap) [Cadangan]"
echo "2) Multi-Binary Flash (0x1000 bootloader, 0x8000 partitions, 0xe000 boot_app0, 0x10000 firmware)"
echo "3) App Firmware Sahaja (0x10000 - Kekalkan data WiFi & tetapan)"
echo ""
read -p "Sila pilih mod [1-3] (Lalai: 1): " CHOICE
CHOICE="${CHOICE:-1}"

if [ "$CHOICE" = "1" ]; then
    echo ""
    echo "[+] Memulakan Full Flash menggunakan fail gabungan (0x0)..."
    $ESPTOOL --chip esp32 --port "$PORT" --baud "$BAUD" \
        --before default_reset --after hard_reset write_flash -z \
        --flash_mode dio --flash_freq 40m --flash_size 4MB \
        0x00000 "${SCRIPT_DIR}/game-minerd-cyd-2432s028-v1.0.0-merged.bin"

elif [ "$CHOICE" = "2" ]; then
    echo ""
    echo "[+] Memulakan Multi-Binary Flash..."
    $ESPTOOL --chip esp32 --port "$PORT" --baud "$BAUD" \
        --before default_reset --after hard_reset write_flash -z \
        --flash_mode dio --flash_freq 40m --flash_size 4MB \
        0x1000 "${SCRIPT_DIR}/bootloader.bin" \
        0x8000 "${SCRIPT_DIR}/partitions.bin" \
        0xe000 "${SCRIPT_DIR}/boot_app0.bin" \
        0x10000 "${SCRIPT_DIR}/firmware.bin"

elif [ "$CHOICE" = "3" ]; then
    echo ""
    echo "[+] Memulakan Flash Firmware Sahaja (0x10000)..."
    $ESPTOOL --chip esp32 --port "$PORT" --baud "$BAUD" \
        --before default_reset --after hard_reset write_flash -z \
        --flash_mode dio --flash_freq 40m --flash_size 4MB \
        0x10000 "${SCRIPT_DIR}/firmware.bin"
else
    echo "[!] Pilihan tidak sah. Dibatalkan."
    exit 1
fi

echo ""
echo "=========================================================="
echo " [OK] Flash Selesai Berjaya!"
echo " Skrin CYD anda akan dimulakan semula secara automatik."
echo " Sambungkan ke WiFi atau layari WebGUI di: http://game-minerd.local"
echo "=========================================================="
