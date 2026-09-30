#ifndef MINER_TASK_H
#define MINER_TASK_H

#include <Arduino.h>

/**
 * @brief Memulakan tugas FreeRTOS Solo Mining yang diikat ke Core 0
 */
void startMinerTask();

/**
 * @brief Gelung perlombongan solo Core 0 (Stratum & SHA-256)
 */
void minerTaskLoop(void* parameter);

/**
 * @brief Menukar mod antara Solo (:3333) dan Pool PPLNS (:13333) di public-pool.io
 */
void toggleMiningPoolMode();

/**
 * @brief Semak adakah sedang dalam mod POOL PPLNS (port 13333)
 */
bool isMiningPoolMode();

/**
 * @brief Jalankan kelompok pengiraan SHA-256 di Core 1 semasa Game Idle (Dual CPU Turbo)
 */
void runMiningWorkerCore1(uint32_t batchSize = 10000);

#include <vector>

struct SdGameItem {
    String filename;      // e.g. "0020 Tank Wars 2008 Accelerated Chinese Version.nes"
    String displayName;   // e.g. "Tank Wars 2008"
    uint32_t sizeKB;      // e.g. 24
    bool isCompatible;    // <= 64KB fits in internal ESP32 SRAM
};

/**
 * @brief Semak status penyambungan kad MicroSD CYD
 */
bool isSdCardMounted();

/**
 * @brief Dapatkan nama folder ROM NES yang dikesan (cth: "/nes")
 */
String getSdDetectedFolder();

/**
 * @brief Dapatkan senarai game NES daripada kad SD
 */
std::vector<SdGameItem> getSdGameList(bool forceRescan = false);

/**
 * @brief Baca fail dari kad SD ke dalam buffer RAM
 * @param sdPath Laluan fail di kad SD (cth: "/nes/0022 Tetris.nes")
 * @param outSize Penunjuk untuk menyimpan saiz fail yang dibaca
 * @return Penunjuk ke buffer (caller mesti free()), atau NULL jika gagal
 */
uint8_t* readSdFileToBuffer(const char* sdPath, size_t* outSize);

/**
 * @brief Semak jika terdapat permintaan pelancaran ROM daripada WebGUI
 * @param outName Penunjuk untuk menerima nama fail ROM
 * @return true jika ada permintaan baru
 */
bool checkPendingRomRequest(String& outName);

#endif // MINER_TASK_H
