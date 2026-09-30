#ifndef MINER_SHARED_DATA_H
#define MINER_SHARED_DATA_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * @brief Struktur data kongsi antara Core 0 (Miner) dan Core 1 (Game HUD & Paparan)
 */
struct MinerStats {
    float currentHashrate;      // Nilai hashrate semasa dalam kH/s
    uint32_t totalHashes;       // Jumlah hashes yang telah diproses
    uint32_t validShares;       // Jumlah share yang sah dihantar ke pool
    double bestDifficulty;      // Rekod kesukaran terbaik (best difficulty)
    bool isWifiConnected;       // Status sambungan WiFi
    bool isPoolConnected;       // Status sambungan Stratum mining pool
    bool blockFoundAlert;       // Isyarat bahawa blok Bitcoin sah telah ditemui!
    char activePool[48];        // Nama pool yang sedang disambungkan
    char btcAddress[64];        // Alamat dompet Bitcoin pelombong
    char wifiSSID[32];          // Nama WiFi (SSID) yang disambungkan
    char ipAddress[24];         // Alamat IP peranti
};

class MinerDataManager {
private:
    MinerStats stats;
    SemaphoreHandle_t mutex;

public:
    MinerDataManager() {
        mutex = xSemaphoreCreateMutex();
        stats.currentHashrate = 0.0f;
        stats.totalHashes = 0;
        stats.validShares = 0;
        stats.bestDifficulty = 0.0;
        stats.isWifiConnected = false;
        stats.isPoolConnected = false;
        stats.blockFoundAlert = false;
        strncpy(stats.activePool, "public-pool.io:21496", sizeof(stats.activePool) - 1);
        strncpy(stats.btcAddress, "bc1q...", sizeof(stats.btcAddress) - 1);
        strncpy(stats.wifiSSID, "Scanning...", sizeof(stats.wifiSSID) - 1);
        strncpy(stats.ipAddress, "0.0.0.0", sizeof(stats.ipAddress) - 1);
    }

    void updateMiningProgress(float hashrate, uint32_t hashesIncrement, double diff) {
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            stats.currentHashrate = hashrate;
            stats.totalHashes += hashesIncrement;
            if (diff > stats.bestDifficulty) {
                stats.bestDifficulty = diff;
            }
            xSemaphoreGive(mutex);
        }
    }

    void setWifiDetails(bool wifi, const char* ssid, const char* ip) {
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            stats.isWifiConnected = wifi;
            if (ssid) {
                strncpy(stats.wifiSSID, ssid, sizeof(stats.wifiSSID) - 1);
            }
            if (ip) {
                strncpy(stats.ipAddress, ip, sizeof(stats.ipAddress) - 1);
            }
            xSemaphoreGive(mutex);
        }
    }

    void setPoolAndWallet(const char* pool, const char* wallet) {
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (pool) {
                strncpy(stats.activePool, pool, sizeof(stats.activePool) - 1);
            }
            if (wallet) {
                strncpy(stats.btcAddress, wallet, sizeof(stats.btcAddress) - 1);
            }
            xSemaphoreGive(mutex);
        }
    }

    void setConnectionStatus(bool wifi, bool pool, const char* poolName = nullptr) {
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            stats.isWifiConnected = wifi;
            stats.isPoolConnected = pool;
            if (poolName) {
                strncpy(stats.activePool, poolName, sizeof(stats.activePool) - 1);
            }
            xSemaphoreGive(mutex);
        }
    }

    void triggerBlockFound() {
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            stats.blockFoundAlert = true;
            stats.validShares++;
            xSemaphoreGive(mutex);
        }
    }

    MinerStats getStats() {
        MinerStats copy;
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            copy = stats;
            xSemaphoreGive(mutex);
        } else {
            // fallback safe copy
            copy = stats;
        }
        return copy;
    }

    bool checkAndClearBlockAlert() {
        bool alerted = false;
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            alerted = stats.blockFoundAlert;
            stats.blockFoundAlert = false;
            xSemaphoreGive(mutex);
        }
        return alerted;
    }
};

extern MinerDataManager g_minerData;

#endif // MINER_SHARED_DATA_H
