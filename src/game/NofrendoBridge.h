#ifndef NOFRENDO_BRIDGE_H
#define NOFRENDO_BRIDGE_H

#include <Arduino.h>
#include "DisplayDriver.h"

/**
 * @brief Inisialisasi bridge perkakasan CYD untuk NoFrendo
 */
void initNofrendoBridge(LGFX* pLcd);

/**
 * @brief Jalankan game ROM NES terpilih
 * @param romPath Laluan fail penuh, cth: "/sd/nes/0020 Tank Wars.nes"
 * @return 0 jika berjaya, negatif jika ralat (cth: memori tidak cukup)
 */
int runNofrendoGame(const char* romPath);

/**
 * @brief Hentikan emulator dan keluar kembali ke menu
 */
void stopNofrendoGame();

/**
 * @brief Lukis bingkai bezel kawalan sentuh D-Pad dan butang A/B pada skrin
 */
void drawNofrendoBezel();

#endif // NOFRENDO_BRIDGE_H
