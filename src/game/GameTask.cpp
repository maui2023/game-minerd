#include "GameTask.h"
#include "Config.h"
#include "DisplayDriver.h"
#include "MinerSharedData.h"
#include "SnakesAndLadders.h"
#include "miner/MinerTask.h"

static LGFX lcd;
static LGFX_Sprite hudSprite(&lcd);
static LGFX_Sprite panelSprite(&lcd);

static SnakesAndLaddersGame game(320, 240);
static AppMode currentMode = MODE_GAME;
static unsigned long lastUserInputTime = 0;
static unsigned long lastHudUpdateTime = 0;
static const unsigned long IDLE_TIMEOUT_MS = 25000; // 25 saat auto beralih ke Mining Dashboard

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
    
    // 1. Hashrate info (Kiri)
    char hrBuf[24];
    snprintf(hrBuf, sizeof(hrBuf), "%.1fk", stats.currentHashrate);
    hudSprite.setTextColor(TFT_GREENYELLOW, hudSprite.color565(15, 23, 42));
    hudSprite.drawString(hrBuf, 4, 7);

    // 2. Butang Sentuh Solo / Pool Mode (x=50..104)
    bool isPool = isMiningPoolMode();
    uint16_t btnColor = isPool ? hudSprite.color565(16, 185, 129) : hudSprite.color565(37, 99, 235);
    hudSprite.fillRoundRect(50, 3, 54, 18, 3, btnColor);
    hudSprite.setTextColor(TFT_WHITE, btnColor);
    hudSprite.drawCenterString(isPool ? "POOL" : "SOLO", 50 + 27, 5);

    // 3. IP Address info (x=110..195)
    char ipBuf[32];
    snprintf(ipBuf, sizeof(ipBuf), "%s", stats.ipAddress);
    hudSprite.setTextColor(stats.isWifiConnected ? TFT_CYAN : TFT_ORANGE, hudSprite.color565(15, 23, 42));
    hudSprite.drawString(ipBuf, 110, 7);

    // 4. WiFi SSID info (x=202..265)
    char wfBuf[24];
    snprintf(wfBuf, sizeof(wfBuf), "%.9s", stats.wifiSSID);
    uint16_t wfColor = stats.isWifiConnected ? TFT_GREEN : TFT_YELLOW;
    hudSprite.setTextColor(wfColor, hudSprite.color565(15, 23, 42));
    hudSprite.drawString(wfBuf, 202, 7);

    // 5. Butang Putar Skrin (x=272..316)
    hudSprite.fillRoundRect(272, 3, 44, 18, 3, hudSprite.color565(51, 65, 85));
    hudSprite.setTextColor(TFT_SILVER, hudSprite.color565(51, 65, 85));
    hudSprite.drawCenterString("ROT", 272 + 22, 5);

    // Tolak sprite ke skrin (Pantas melalui DMA, sifar flicker)
    hudSprite.pushSprite(0, 0);
}

static void drawMinerDashboard(const MinerStats& stats, bool fullRedraw = true) {
    int cardW = (lcd.width() - 30) / 2;
    int card2X = 10 + cardW + 10;
    int statsY = 94;

    // Lukis rangka penuh hanya apabila mod baru bermula (Elak Flicker!)
    if (fullRedraw) {
        lcd.fillScreen(lcd.color565(10, 15, 29)); // Midnight Dark

        // Header
        lcd.fillRect(0, 0, lcd.width(), 32, lcd.color565(24, 34, 58));
        lcd.setTextColor(TFT_GOLD, lcd.color565(24, 34, 58));
        lcd.setTextSize(1);
        lcd.drawCenterString("=== PUBLIC-POOL.IO + GAME-MINERD ===", lcd.width() / 2, 5);

        // Kad 1: Hashrate (Kiri atas)
        lcd.fillRect(10, 36, cardW, 52, lcd.color565(15, 23, 42));
        lcd.drawRect(10, 36, cardW, 52, stats.isDualCpuActive ? TFT_GOLD : lcd.color565(56, 189, 248));

        // Kad 2: WiFi & Network (Kanan atas)
        lcd.fillRect(card2X, 36, cardW, 52, lcd.color565(15, 23, 42));
        lcd.drawRect(card2X, 36, cardW, 52, stats.isWifiConnected ? TFT_GREEN : TFT_ORANGE);
        lcd.setTextColor(TFT_WHITE, lcd.color565(15, 23, 42));
        lcd.drawString("RANGKAIAN (WIFI)", card2X + 6, 40);

        // Kad 3: Detail Statistik Perlombongan
        lcd.fillRect(10, statsY, lcd.width() - 20, 74, lcd.color565(15, 23, 42));
        lcd.drawRect(10, statsY, lcd.width() - 20, 74, lcd.color565(71, 85, 105));
        lcd.setTextColor(TFT_GOLD, lcd.color565(15, 23, 42));
        lcd.drawString("STATISTIK PERLOMBONGAN BITCOIN", 16, statsY + 5);
        lcd.drawFastHLine(16, statsY + 16, lcd.width() - 32, lcd.color565(51, 65, 85));

        // Butang Sentuh Besar: Pertukaran Mod Solo <-> Pool PPLNS
        bool isPool = isMiningPoolMode();
        uint16_t btnBg = isPool ? lcd.color565(16, 185, 129) : lcd.color565(37, 99, 235);
        lcd.fillRoundRect(10, 174, lcd.width() - 20, 36, 6, btnBg);
        lcd.drawRoundRect(10, 174, lcd.width() - 20, 36, 6, TFT_WHITE);

        if (isPool) {
            lcd.setTextColor(TFT_WHITE, btnBg);
            lcd.drawCenterString("[ MOD POOL PPLNS (Port 13333) ]", lcd.width() / 2, 178);
            lcd.setTextColor(TFT_YELLOW, btnBg);
            lcd.drawCenterString(">> SENTUH UNTUK TUKAR KE SOLO (3333) <<", lcd.width() / 2, 193);
        } else {
            lcd.setTextColor(TFT_WHITE, btnBg);
            lcd.drawCenterString("[ MOD SOLO MINING (Port 3333) ]", lcd.width() / 2, 178);
            lcd.setTextColor(TFT_YELLOW, btnBg);
            lcd.drawCenterString(">> SENTUH UNTUK TUKAR KE POOL (13333) <<", lcd.width() / 2, 193);
        }

        // Footer Hint
        lcd.fillRect(0, 218, lcd.width(), 22, lcd.color565(15, 23, 42));
        lcd.setTextColor(TFT_SILVER, lcd.color565(15, 23, 42));
        lcd.drawCenterString(">> Sentuh Bawah Untuk Kembali Main Ular & Tangga <<", lcd.width() / 2, 224);
    }

    // Kemas kini nombor dan status sahaja secara lancar tanpa menghapus skrin (Smooth & Zero Flicker)
    lcd.startWrite();

    // 1. Sub-garisan Header Dual CPU
    lcd.fillRect(0, 17, lcd.width(), 13, lcd.color565(24, 34, 58));
    lcd.setTextColor(stats.isDualCpuActive ? TFT_GOLD : TFT_SILVER, lcd.color565(24, 34, 58));
    lcd.drawCenterString(stats.isDualCpuActive ? ">> DUAL CPU MAX HASH ACTIVE (CORE 0 + CORE 1) <<" : "Background Solo/Pool Stratum Mining (Core 0)", lcd.width() / 2, 18);

    // 2. Kad 1: Hashrate
    lcd.drawRect(10, 36, cardW, 52, stats.isDualCpuActive ? TFT_GOLD : lcd.color565(56, 189, 248));
    lcd.fillRect(12, 38, cardW - 4, 14, lcd.color565(15, 23, 42));
    lcd.setTextColor(stats.isDualCpuActive ? TFT_GOLD : TFT_SKYBLUE, lcd.color565(15, 23, 42));
    lcd.drawString(stats.isDualCpuActive ? "DUAL CPU MAX HASH" : "HASHRATE AKTIF", 16, 40);

    char hrBuf[32];
    snprintf(hrBuf, sizeof(hrBuf), "%.2f kH/s", stats.currentHashrate);
    lcd.fillRect(14, 54, cardW - 8, 22, lcd.color565(15, 23, 42));
    lcd.setTextColor(stats.isDualCpuActive ? TFT_GREEN : TFT_GREENYELLOW, lcd.color565(15, 23, 42));
    lcd.setTextSize(2);
    lcd.drawString(hrBuf, 16, 56);
    lcd.setTextSize(1);

    // 3. Kad 2: WiFi
    char ssidLine[32];
    snprintf(ssidLine, sizeof(ssidLine), "SSID: %.14s", stats.wifiSSID);
    char ipLine[32];
    snprintf(ipLine, sizeof(ipLine), "IP  : %s", stats.ipAddress);
    lcd.fillRect(card2X + 6, 52, cardW - 12, 32, lcd.color565(15, 23, 42));
    lcd.setTextColor(stats.isWifiConnected ? TFT_GREEN : TFT_YELLOW, lcd.color565(15, 23, 42));
    lcd.drawString(ssidLine, card2X + 6, 54);
    lcd.setTextColor(stats.isWifiConnected ? TFT_CYAN : TFT_RED, lcd.color565(15, 23, 42));
    lcd.drawString(ipLine, card2X + 6, 68);

    // 4. Kad 3: Detail Statistik Perlombongan
    char line1[64];
    snprintf(line1, sizeof(line1), "Jumlah Hash: %-9u | Valid Shares: %u", stats.totalHashes, stats.validShares);
    char line2[64];
    snprintf(line2, sizeof(line2), "Best Diff  : %-9.2f | Pool: %s", stats.bestDifficulty, stats.activePool);
    char line3[64];
    snprintf(line3, sizeof(line3), "Dompet BTC : %.16s...", stats.btcAddress);

    lcd.fillRect(14, statsY + 20, lcd.width() - 28, 48, lcd.color565(15, 23, 42));
    lcd.setTextColor(TFT_WHITE, lcd.color565(15, 23, 42));
    lcd.drawString(line1, 16, statsY + 22);
    lcd.drawString(line2, 16, statsY + 36);
    lcd.drawString(line3, 16, statsY + 50);

    lcd.endWrite();
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
    panelSprite.createSprite(78, 204);

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

        int32_t touchX = 0, touchY = 0;
        bool touched = lcd.getTouch(&touchX, &touchY);
        if (touched && (touchX < 0 || touchX >= 320 || touchY < 0 || touchY >= 240)) {
            touched = false;
        }
        bool btnBoot = (digitalRead(PIN_BTN_LEFT) == LOW);

        if (touched || btnBoot) {
            lastUserInputTime = millis();

            if (currentMode == MODE_GAME && touched) {
                // Sentuhan di bar HUD atas (touchY < 25)
                if (touchY < 25) {
                    // Butang Tukar Mod Solo <-> Pool (x=45..108)
                    if (touchX >= 45 && touchX <= 108) {
                        toggleMiningPoolMode();
                        drawMiniMiningHUD(g_minerData.getStats());
                        vTaskDelay(pdMS_TO_TICKS(350));
                        continue;
                    }
                    // Butang Putar Skrin 180 darjah (x > 265)
                    if (touchX > 265) {
                        currentRotation = (currentRotation == 3) ? 1 : 3;
                        lcd.setRotation(currentRotation);
                        lcd.fillScreen(TFT_BLACK);
                        game.needBoardRedraw = true;
                        game.needPanelRedraw = true;
                        modeSwitched = true;
                        vTaskDelay(pdMS_TO_TICKS(400));
                        continue;
                    }
                }

                // Kawasan permainan Ular & Tangga: baling dadu
                if (game.handleTouch(touchX, touchY)) {
                    vTaskDelay(pdMS_TO_TICKS(180));
                }
            } else if (currentMode == MODE_MINER_DASHBOARD && touched) {
                // Butang Tukar Mod Solo <-> Pool di Dashboard (y=170..214)
                if (touchY >= 170 && touchY <= 214) {
                    toggleMiningPoolMode();
                    drawMinerDashboard(g_minerData.getStats(), true);
                    vTaskDelay(pdMS_TO_TICKS(350));
                    continue;
                }

                // Sentuh di luar butang mod untuk kembali ke game
                currentMode = MODE_GAME;
                lcd.fillScreen(TFT_BLACK);
                game.needBoardRedraw = true;
                game.needPanelRedraw = true;
                modeSwitched = true;
                vTaskDelay(pdMS_TO_TICKS(250));
                continue;
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
                if (currentMode == MODE_GAME && game.canRoll()) {
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
                panelSprite.pushSprite(240, 28);
                modeSwitched = false;
            }

            // Kemas kini pergerakan game (hanya kemas kini petak yang terjejas)
            game.update(lcd);

            // Lukis panel kanan melalui sprite jika ada perubahan
            if (game.needPanelRedraw) {
                game.renderControlPanel(panelSprite);
                panelSprite.pushSprite(240, 28);
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
            // Mod Mining Dashboard Penuh: GAME SEDANG IDLE!
            // Optimumkan DUAL CPU untuk hashrate maksimum di Core 1 (Kelompok 10,000 nonces)
            runMiningWorkerCore1(10000);

            // Kemas kini paparan Dashboard setiap 1 saat (Sifar Flicker)
            if (modeSwitched || (millis() - lastHudUpdateTime > 1000)) {
                drawMinerDashboard(g_minerData.getStats(), modeSwitched);
                lastHudUpdateTime = millis();
                modeSwitched = false;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
}
