#ifndef GAME_MINERD_CONFIG_H
#define GAME_MINERD_CONFIG_H

#include <Arduino.h>

// ==============================================================================
// 1. HARDWARE PIN DEFINITIONS (Berasaskan Sasaran Papan)
// ==============================================================================

#if defined(BOARD_CYD_2432S028)
    // ESP32-2432S028R ("Cheap Yellow Display" 2.8" ILI9341)
    #define TFT_MISO       12
    #define TFT_MOSI       13
    #define TFT_SCLK       14
    #define TFT_CS         15
    #define TFT_DC          2
    #define TFT_RST        -1
    #define TFT_BL         21

    // Butang Kawalan (Butang BOOT fizikal + Pin Tambahan / Touch)
    #define PIN_BTN_LEFT    0    // Boot button (juga boleh digunakan untuk action)
    #define PIN_BTN_RIGHT  35    // CN1 pin / Analog
    #define PIN_BTN_ACTION 27    // CN1 IO27
    #define PIN_BTN_MODE   22    // CN1 IO22 (Toggle Game <-> Mining Dashboard)

    // RGB Onboard LED
    #define PIN_LED_RED     4
    #define PIN_LED_GREEN  16
    #define PIN_LED_BLUE   17

    // Audio Speaker / Buzzer
    #define PIN_BUZZER     26

#elif defined(BOARD_TTGO_TDISPLAY)
    // LILYGO TTGO T-Display 1.14" ST7789
    #define TFT_MOSI       19
    #define TFT_SCLK       18
    #define TFT_CS          5
    #define TFT_DC         16
    #define TFT_RST        23
    #define TFT_BL          4

    // 2 Butang Terbina
    #define PIN_BTN_LEFT    0    // Button 1 (GPIO 0)
    #define PIN_BTN_RIGHT  35    // Button 2 (GPIO 35)
    #define PIN_BTN_ACTION 35
    #define PIN_BTN_MODE    0

    #define PIN_BUZZER     -1

#else
    // Generic ESP32 DevKit V1 (Sambungan Breadboard / Custom Shield)
    #define TFT_MISO       19
    #define TFT_MOSI       23
    #define TFT_SCLK       18
    #define TFT_CS          5
    #define TFT_DC          2
    #define TFT_RST         4
    #define TFT_BL         15

    #define PIN_BTN_LEFT   12
    #define PIN_BTN_RIGHT  14
    #define PIN_BTN_ACTION 27
    #define PIN_BTN_MODE    0    // BOOT button to toggle modes

    #define PIN_BUZZER     25
#endif

// ==============================================================================
// 2. DEFAULT SOLO MINING CONFIGURATION
// ==============================================================================
#define DEFAULT_WIFI_SSID       "MiQaNoMeira_2.4G"
#define DEFAULT_WIFI_PASS       "Komando@2023"

// Default Mining Pool (public-pool.io: Solo = 3333, PPLNS Pool = 13333)
#define DEFAULT_POOL_URL        "public-pool.io"
#define DEFAULT_POOL_PORT       3333
#define DEFAULT_POOL_PPLNS_PORT 13333
#define DEFAULT_BTC_WALLET      "bc1q065llaash5vkv06v3zmts8jte8yuz6j7qtrnee"

// ==============================================================================
// 3. FREERTOS TASK ALLOCATION
// ==============================================================================
#define CORE_MINING             0    // Core 0 dikhaskan untuk Stratum Solo Mining & WiFi
#define CORE_GAMING             1    // Core 1 dikhaskan untuk Enjin Permainan & Paparan GFX

#define PRIORITY_MINING         2    // Keutamaan biasa untuk kerja hashrate di Core 0
#define PRIORITY_GAMING         3    // Keutamaan tinggi di Core 1 untuk memastikan 60 FPS lancar

#endif // GAME_MINERD_CONFIG_H
