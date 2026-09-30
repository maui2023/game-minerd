#ifndef GAME_TASK_H
#define GAME_TASK_H

#include <Arduino.h>

enum AppMode {
    MODE_GAME = 0,
    MODE_MINER_DASHBOARD = 1
};

/**
 * @brief Memulakan tugas FreeRTOS Enjin Permainan & Paparan yang diikat ke Core 1
 */
void startGameTask();

/**
 * @brief Gelung pemprosesan grafik, kawalan permainan, dan HUD di Core 1
 */
void gameTaskLoop(void* parameter);

#endif // GAME_TASK_H
