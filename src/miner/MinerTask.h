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

#endif // MINER_TASK_H
