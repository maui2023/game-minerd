#include "GameTask.h"
#include "Config.h"
#include "DisplayDriver.h"
#include "MinerSharedData.h"
#include "SnakesAndLadders.h"

static LGFX lcd;
static LGFX_Sprite hudSprite(&lcd);
static LGFX_Sprite panelSprite(&lcd);

static SnakesAndLaddersGame game(320, 240);
static AppMode currentMode = MODE_GAME;
static unsigned long lastUserInputTime = 0;
static unsigned long lastHudUpdateTime = 0;
static const unsigned long IDLE_TIMEOUT_MS = 40000; // 40 saat auto beralih ke Mining Dashboard

static uint8_t currentRotation = 3; // Landscape rotasi 3 (pusing 180 darjah dari 1)

void startGameTask() {
    xTaskCreatePinnedToCore(
        gameTaskLoop,
        "GameTask",
        10240,
        NULL,
        PRIORITY_GAMING,
        NULL,
        CORE_GAMING
    );
}

static void drawMiniMiningHUD(const MinerStats& stats) {
    hudSprite.fillScreen(hudSprite.color565(15, 23, 42)); // Dark Slate
    hudSprite.drawFastHLine(0, 23, hudSprite.width(), hudSprite.color565(56, 189, 248)); // Cyan border

    hudSprite.setTextSize(1);
    
    // 1. Hashrate info
    char hrBuf[32];
    snprintf(hrBuf, sizeof(hrBuf), "[*] %.1f kH", stats.currentHashrate);
    hudSprite.setTextColor(TFT_GREENYELLOW, hudSprite.color565(15, 23, 42));
    hudSprite.drawString(hrBuf, 4, 7);

    // 2. IP Address info
    char ipBuf[40];
    snprintf(ipBuf, sizeof(ipBuf), "IP: %s", stats.ipAddress);
    hudSprite.setTextColor(stats.isWifiConnected ? TFT_CYAN : TFT_ORANGE, hudSprite.color565(15, 23, 42));
    hudSprite.drawString(ipBuf, 85, 7);

    // 3. WiFi SSID info (Sebelah kanan)
    char wfBuf[32];
    snprintf(wfBuf, sizeof(wfBuf), "WF: %.10s", stats.wifiSSID);
    uint16_t wfColor = stats.isWifiConnected ? TFT_GREEN : TFT_YELLOW;
    hudSprite.setTextColor(wfColor, hudSprite.color565(15, 23, 42));
    hudSprite.drawString(wfBuf, hudSprite.width() - 88, 7);

    // Tolak sprite ke skrin (Pantas melalui DMA, sifar flicker)
    hudSprite.pushSprite(0, 0);
}

static void drawMinerDashboard(const MinerStats& stats) {
    lcd.fillScreen(lcd.color565(10, 15, 29)); // Midnight Dark

    // Header
    lcd.fillRect(0, 0, lcd.width(), 32, lcd.color565(24, 34, 58));
    lcd.setTextColor(TFT_GOLD, lcd.color565(24, 34, 58));
    lcd.setTextSize(1);
    lcd.drawCenterString("=== NERDMINER + GAME-MINERD ===", lcd.width() / 2, 5);
    lcd.setTextColor(TFT_SILVER, lcd.color565(24, 34, 58));
    lcd.drawCenterString("Background Solo Mining Engine Active (Core 0)", lcd.width() / 2, 18);

    // Kad 1: Hashrate (Kiri atas)
    int cardW = (lcd.width() - 30) / 2;
    lcd.fillRect(10, 38, cardW, 56, lcd.color565(15, 23, 42));
    lcd.drawRect(10, 38, cardW, 56, lcd.color565(56, 189, 248));
    lcd.setTextColor(TFT_SKYBLUE, lcd.color565(15, 23, 42));
    lcd.drawString("SOLO HASHRATE", 16, 44);
    char hrBuf[32];
    snprintf(hrBuf, sizeof(hrBuf), "%.2f kH/s", stats.currentHashrate);
    lcd.setTextColor(TFT_GREENYELLOW, lcd.color565(15, 23, 42));
    lcd.setTextSize(2);
    lcd.drawString(hrBuf, 16, 62);
    lcd.setTextSize(1);

    // Kad 2: WiFi & Network (Kanan atas)
    int card2X = 10 + cardW + 10;
    lcd.fillRect(card2X, 38, cardW, 56, lcd.color565(15, 23, 42));
    lcd.drawRect(card2X, 38, cardW, 56, stats.isWifiConnected ? TFT_GREEN : TFT_ORANGE);
    lcd.setTextColor(TFT_WHITE, lcd.color565(15, 23, 42));
    lcd.drawString("RANGKAIAN (WIFI)", card2X + 6, 44);
    
    char ssidLine[32];
    snprintf(ssidLine, sizeof(ssidLine), "SSID: %.14s", stats.wifiSSID);
    lcd.setTextColor(stats.isWifiConnected ? TFT_GREEN : TFT_YELLOW, lcd.color565(15, 23, 42));
    lcd.drawString(ssidLine, card2X + 6, 58);

    char ipLine[32];
    snprintf(ipLine, sizeof(ipLine), "IP  : %s", stats.ipAddress);
    lcd.setTextColor(stats.isWifiConnected ? TFT_CYAN : TFT_RED, lcd.color565(15, 23, 42));
    lcd.drawString(ipLine, card2X + 6, 72);

    // Kad 3: Detail Statistik Perlombongan (Bawah)
    int statsY = 100;
    lcd.fillRect(10, statsY, lcd.width() - 20, 105, lcd.color565(15, 23, 42));
    lcd.drawRect(10, statsY, lcd.width() - 20, 105, lcd.color565(71, 85, 105));

    lcd.setTextColor(TFT_GOLD, lcd.color565(15, 23, 42));
    lcd.drawString("STATISTIK SOLO MINING BITCOIN", 18, statsY + 6);
    lcd.drawFastHLine(18, statsY + 18, lcd.width() - 36, lcd.color565(51, 65, 85));

    lcd.setTextColor(TFT_WHITE, lcd.color565(15, 23, 42));
    char line[64];
    snprintf(line, sizeof(line), "Jumlah Hash    : %u", stats.totalHashes);
    lcd.drawString(line, 18, statsY + 24);

    snprintf(line, sizeof(line), "Valid Shares   : %u", stats.validShares);
    lcd.drawString(line, 18, statsY + 38);

    snprintf(line, sizeof(line), "Best Difficulty: %.2f", stats.bestDifficulty);
    lcd.drawString(line, 18, statsY + 52);

    snprintf(line, sizeof(line), "Mining Pool    : %s", stats.activePool);
    lcd.drawString(line, 18, statsY + 66);

    snprintf(line, sizeof(line), "Status WiFi    : %s", stats.isWifiConnected ? "ONLINE (Disambung)" : "Menyambung...");
    lcd.setTextColor(stats.isWifiConnected ? TFT_GREEN : TFT_ORANGE, lcd.color565(15, 23, 42));
    lcd.drawString(line, 18, statsY + 80);

    // Footer Hint
    lcd.fillRect(0, lcd.height() - 24, lcd.width(), 24, lcd.color565(30, 41, 59));
    lcd.setTextColor(TFT_YELLOW, lcd.color565(30, 41, 59));
    lcd.drawCenterString(">> Sentuh Skrin Untuk Kembali Main Ular & Tangga <<", lcd.width() / 2, lcd.height() - 17);
}

void gameTaskLoop(void* parameter) {
    Serial.println("[CORE 1] GameTask dimulakan: Ular & Tangga Touch Edition di Core 1");

    // Inisialisasi Paparan LCD LovyanGFX
    lcd.init();
    lcd.setRotation(currentRotation); // Landscape 320x240 (Rotasi 3)
    lcd.setBrightness(220);
    lcd.fillScreen(TFT_BLACK);

    // Cipta Sprite di RAM untuk penghapusan flicker
    hudSprite.setColorDepth(16);
    hudSprite.createSprite(320, 24);

    panelSprite.setColorDepth(16);
    panelSprite.createSprite(106, 204);

    // Konfigurasi butang fizikal BOOT
    pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
#if defined(PIN_BTN_MODE) && (PIN_BTN_MODE >= 0)
    pinMode(PIN_BTN_MODE, INPUT_PULLUP);
#endif

    lastUserInputTime = millis();
    lastHudUpdateTime = 0;
    unsigned long bootPressStart = 0;
    bool modeSwitched = true;

    while (true) {
        MinerStats stats = g_minerData.getStats();

        // 1. Pengesanan Sentuhan Skrin & Butang BOOT
        int32_t touchX = 0, touchY = 0;
        bool touched = lcd.getTouch(&touchX, &touchY);
        bool btnBoot = (digitalRead(PIN_BTN_LEFT) == LOW);

        if (touched || btnBoot) {
            lastUserInputTime = millis();

            // Pintasan Sentuhan: Sentuh penjuru kanan HUD untuk putar skrin 180 darjah
            if (touched && touchY < 25 && touchX > 220) {
                currentRotation = (currentRotation == 3) ? 1 : 3;
                lcd.setRotation(currentRotation);
                lcd.fillScreen(TFT_BLACK);
                game.needBoardRedraw = true;
                game.needPanelRedraw = true;
                modeSwitched = true;
                vTaskDelay(pdMS_TO_TICKS(400));
                continue;
            }

            // Jika dalam mod dashboard dan skrin disentuh, tukar kembali ke Game
            if (currentMode == MODE_MINER_DASHBOARD) {
                currentMode = MODE_GAME;
                lcd.fillScreen(TFT_BLACK);
                game.needBoardRedraw = true;
                game.needPanelRedraw = true;
                modeSwitched = true;
                vTaskDelay(pdMS_TO_TICKS(250));
                continue;
            }

            // Dalam Mod Permainan: Sentuh untuk baling dadu
            if (currentMode == MODE_GAME) {
                if (touched) {
                    if (game.handleTouch(touchX, touchY)) {
                        vTaskDelay(pdMS_TO_TICKS(180));
                    }
                }
            }
        }

        // Kendalian Butang BOOT fizikal (Tekan pendek = Baling Dadu, Tekan > 1.2s = Putar Skrin)
        if (btnBoot) {
            if (bootPressStart == 0) bootPressStart = millis();
            if (millis() - bootPressStart > 1200) {
                currentRotation = (currentRotation == 3) ? 1 : 3;
                lcd.setRotation(currentRotation);
                lcd.fillScreen(TFT_BLACK);
                game.needBoardRedraw = true;
                game.needPanelRedraw = true;
                modeSwitched = true;
                bootPressStart = 0;
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
        } else {
            if (bootPressStart > 0 && millis() - bootPressStart < 1200) {
                if (currentMode == MODE_GAME && game.isPlayerTurn && !game.isRolling) {
                    game.rollDice();
                } else if (currentMode == MODE_MINER_DASHBOARD) {
                    currentMode = MODE_GAME;
                    lcd.fillScreen(TFT_BLACK);
                    game.needBoardRedraw = true;
                    game.needPanelRedraw = true;
                    modeSwitched = true;
                }
            }
            bootPressStart = 0;
        }

        // Auto Screensaver ke Mining Dashboard jika idle 40 saat
        if (currentMode == MODE_GAME && (millis() - lastUserInputTime > IDLE_TIMEOUT_MS)) {
            currentMode = MODE_MINER_DASHBOARD;
            modeSwitched = true;
        }

        // 2. Pemprosesan Render Mengikut Mod (Sifar Flicker)
        if (currentMode == MODE_GAME) {
            // Lukis papan hanya sekali apabila diperlukan
            if (game.needBoardRedraw || modeSwitched) {
                lcd.fillScreen(TFT_BLACK);
                game.drawFullBoard(lcd);
                drawMiniMiningHUD(stats);
                game.renderControlPanel(panelSprite);
                panelSprite.pushSprite(212, 28);
                modeSwitched = false;
            }

            // Kemas kini pergerakan game (hanya kemas kini petak yang terjejas)
            game.update(lcd);

            // Lukis panel kanan melalui sprite jika ada perubahan
            if (game.needPanelRedraw) {
                game.renderControlPanel(panelSprite);
                panelSprite.pushSprite(212, 28);
            }

            // Kemas kini HUD mini setiap 1 saat
            if (millis() - lastHudUpdateTime > 1000) {
                drawMiniMiningHUD(stats);
                lastHudUpdateTime = millis();
            }

            // 3. Semak Penggera Penemuan Blok Bitcoin
            if (g_minerData.checkAndClearBlockAlert()) {
                lcd.fillRect(20, 60, lcd.width() - 40, 100, TFT_GOLD);
                lcd.drawRect(20, 60, lcd.width() - 40, 100, TFT_RED);
                lcd.setTextColor(TFT_BLACK, TFT_GOLD);
                lcd.setTextSize(2);
                lcd.drawCenterString("BLOCK FOUND!!", lcd.width() / 2, 80);
                lcd.setTextSize(1);
                lcd.drawCenterString("Ganjaran 3.125 BTC Diperolehi!", lcd.width() / 2, 115);
                vTaskDelay(pdMS_TO_TICKS(3000));
                game.needBoardRedraw = true;
                game.needPanelRedraw = true;
            }

            vTaskDelay(pdMS_TO_TICKS(20));
        } else {
            // Mod Mining Dashboard Penuh (Kemas kini setiap 1 saat)
            if (modeSwitched || (millis() - lastHudUpdateTime > 1000)) {
                drawMinerDashboard(stats);
                lastHudUpdateTime = millis();
                modeSwitched = false;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}
