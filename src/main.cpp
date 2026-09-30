#include <Arduino.h>
#include "Config.h"
#include "miner/MinerTask.h"
#include "game/GameTask.h"

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n========================================================");
    Serial.println("   ___   _   __  __ ___   __  __ ___ _  _ ___ ___ ___   ");
    Serial.println("  / __| /_\\ |  \\/  | __| |  \\/  |_ _| \\| | __| _ \\   \\  ");
    Serial.println(" | (_ |/ _ \\| |\\/| | _|  | |\\/| || || .` | _||   / |) | ");
    Serial.println("  \\___/_/ \\_\\_|  |_|___| |_|  |_|___|_|\\_|___|_|_\\___/  ");
    Serial.println("  Handheld Retro Gaming Console + Background Solo Miner ");
    Serial.println("========================================================\n");

    Serial.printf("[SYSTEM] Chip Model    : %s\n", ESP.getChipModel());
    Serial.printf("[SYSTEM] CPU Cores     : %d cores\n", ESP.getChipCores());
    Serial.printf("[SYSTEM] CPU Frequency : %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("[SYSTEM] Free Heap     : %d bytes\n", ESP.getFreeHeap());

    // 1. Mulakan Tugas Solo Mining di CORE 0 (Background)
    Serial.println("[SYSTEM] Melancarkan MinerTask di Core 0...");
    startMinerTask();

    // 2. Mulakan Tugas Retro Gaming & HUD di CORE 1 (Foreground)
    Serial.println("[SYSTEM] Melancarkan GameTask di Core 1...");
    startGameTask();

    Serial.println("[SYSTEM] Inisialisasi dwi-teras selesai. Kedua-dua teras aktif!");
}

void loop() {
    // FreeRTOS menguruskan Core 0 dan Core 1 secara autonomi melalui task pinning.
    // Gelung utama dibiarkan berehat untuk menjimatkan sumber pengaturcaraan.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
