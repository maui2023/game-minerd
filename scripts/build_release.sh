#!/bin/bash
# ==============================================================================
# Skrip Kompilasi & Penjanaan Fail Pelepasan (Release Builder)
# Sasaran: ESP32 CYD 2.8" (ESP32-2432S028R)
# ==============================================================================

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

echo "=== 1. Memulakan Kompilasi Firmware (PlatformIO) ==="
pio run -e esp32_cyd_2432s028

echo ""
echo "=== 2. Menyediakan Direktori Pelepasan (release/) ==="
mkdir -p release/

BUILD_DIR=".pio/build/esp32_cyd_2432s028"
FRAMEWORK_BOOT_APP0="$(find ~/.platformio/packages/framework-arduinoespressif32 -name 'boot_app0.bin' | head -n 1)"

if [ -z "$FRAMEWORK_BOOT_APP0" ]; then
    echo "[!] Ralat: boot_app0.bin tidak ditemui dalam pakej PlatformIO!"
    exit 1
fi

cp "${BUILD_DIR}/bootloader.bin" release/bootloader.bin
cp "${BUILD_DIR}/partitions.bin" release/partitions.bin
cp "$FRAMEWORK_BOOT_APP0" release/boot_app0.bin
cp "${BUILD_DIR}/firmware.bin" release/firmware.bin

echo "Fail binari asas disalin:"
ls -lh release/*.bin

echo ""
echo "=== 3. Menggabungkan Binari Menjadi Satu Fail Penuh (Merged Bin) ==="
ESPTOOL="$(find ~/.platformio/packages/tool-esptoolpy -name 'esptool.py' | head -n 1)"

if [ -z "$ESPTOOL" ]; then
    ESPTOOL="esptool.py"
else
    ESPTOOL="python3 $ESPTOOL"
fi

$ESPTOOL --chip esp32 merge_bin \
  --output release/game-minerd-cyd-2432s028-v1.0.0-merged.bin \
  --flash_mode dio \
  --flash_freq 40m \
  --flash_size 4MB \
  --target-offset 0x0 \
  0x1000 release/bootloader.bin \
  0x8000 release/partitions.bin \
  0xe000 release/boot_app0.bin \
  0x10000 release/firmware.bin

echo ""
echo "=== 4. Menjana Semula Kod Semakan SHA256 (SHA256SUMS.txt) ==="
(cd release && sha256sum *.bin > SHA256SUMS.txt && cat SHA256SUMS.txt)

echo ""
echo "=========================================================="
echo " [BERJAYA] Pakej pelepasan siap di dalam folder release/!"
echo " Fail sedia untuk dimuat naik ke GitHub Releases / Web Flasher:"
echo " -> release/game-minerd-cyd-2432s028-v1.0.0-merged.bin"
echo "=========================================================="
