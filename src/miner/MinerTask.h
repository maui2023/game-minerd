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

#endif // MINER_TASK_H
