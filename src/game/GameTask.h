#ifndef GAME_TASK_H
#define GAME_TASK_H

#include <Arduino.h>

enum AppMode {
    MODE_GAME_MENU = 0,       // Menu Pemilihan Game (1. RETRO NES | 2. ULAR & TANGGA | 3. DASHBOARD)
    MODE_NES_SELECT = 1,      // Katalog Game NES (Pilih Game dari Kad SD / Demo)
    MODE_GAME_SNAKES = 2,     // Game Ular & Tangga (1-4 Pemain)
    MODE_GAME_NES = 3,        // Emulator Retro NES (NoFrendo Real Game / Built-in Demo)
    MODE_MINER_DASHBOARD = 4  // Mining Dashboard Solo/Pool
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
