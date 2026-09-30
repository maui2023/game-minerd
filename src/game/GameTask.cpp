#include "GameTask.h"
#include "Config.h"
#include "DisplayDriver.h"
#include "MinerSharedData.h"
#include "SnakesAndLadders.h"
#include "NesGame.h"
#include "miner/MinerTask.h"
#include "NofrendoBridge.h"

static LGFX lcd;
static LGFX_Sprite hudSprite(&lcd);
static LGFX_Sprite panelSprite(&lcd);

static SnakesAndLaddersGame game(320, 240);
static NesGameEngine nesGame;
static AppMode currentMode = MODE_GAME_MENU;
static unsigned long lastUserInputTime = 0;
static unsigned long lastHudUpdateTime = 0;
static const unsigned long IDLE_TIMEOUT_MS = 25000; // 25 saat auto beralih ke Mining Dashboard

static int s_nesPage = 0;
static const int NES_ITEMS_PER_PAGE = 4;
static std::vector<SdGameItem> s_cachedGames;
static bool s_isRealRomPlaying = false;

static uint8_t currentRotation = 3; // Landscape rotasi 3 (pusing 180 darjah dari 1)

void startGameTask() {
    xTaskCreatePinnedToCore(
        gameTaskLoop,
        "GameTask",
        20480, // 20KB stack untuk enjin NoFrendo & FreeRTOS
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

    // 5. Butang Kembali Ke Game Hub (x=272..316)
    hudSprite.fillRoundRect(272, 3, 44, 18, 3, hudSprite.color565(225, 29, 72));
    hudSprite.setTextColor(TFT_WHITE, hudSprite.color565(225, 29, 72));
    hudSprite.drawCenterString("HUB", 272 + 22, 5);

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
        lcd.drawCenterString(">> Sentuh Bawah Untuk Kembali Ke Game Hub (1 NES | 2 SNAKE) <<", lcd.width() / 2, 224);
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

static void drawGameHubMenu(const MinerStats& stats) {
    lcd.startWrite();
    lcd.fillScreen(lcd.color565(10, 15, 29)); // Midnight Dark

    // 1. Header Bar
    lcd.fillRect(0, 0, lcd.width(), 32, lcd.color565(24, 34, 58));
    lcd.drawFastHLine(0, 32, lcd.width(), lcd.color565(56, 189, 248));
    lcd.setTextColor(TFT_GOLD, lcd.color565(24, 34, 58));
    lcd.setTextSize(1);
    lcd.drawCenterString("=== GAME HUB: PILIH PERMAINAN ===", lcd.width() / 2, 5);

    char subHeader[64];
    snprintf(subHeader, sizeof(subHeader), "Mining: %.1fkH/s | IP: %s | Pool: %s", 
             stats.currentHashrate, stats.ipAddress, isMiningPoolMode() ? "PPLNS" : "SOLO");
    lcd.setTextColor(TFT_CYAN, lcd.color565(24, 34, 58));
    lcd.drawCenterString(subHeader, lcd.width() / 2, 18);

    // 2. Kad Pilihan 1: RETRO NES 8-BIT (y=40..106, x=16..304)
    lcd.fillRoundRect(16, 40, 288, 66, 8, lcd.color565(15, 23, 42));
    lcd.drawRoundRect(16, 40, 288, 66, 8, lcd.color565(56, 189, 248)); // Cyan border

    // Badge NES
    lcd.fillRoundRect(24, 48, 50, 50, 6, lcd.color565(225, 29, 72)); // Rose Red NES Box
    lcd.drawRoundRect(24, 48, 50, 50, 6, TFT_WHITE);
    lcd.setTextColor(TFT_WHITE, lcd.color565(225, 29, 72));
    lcd.setTextSize(2);
    lcd.drawCenterString("NES", 49, 56);
    lcd.setTextSize(1);
    lcd.drawCenterString("8-BIT", 49, 78);

    // Teks Kad 1
    lcd.setTextColor(TFT_WHITE, lcd.color565(15, 23, 42));
    lcd.setTextSize(1);
    lcd.drawString("1. RETRO NES EMULATOR", 82, 48);
    lcd.setTextColor(TFT_GREENYELLOW, lcd.color565(15, 23, 42));
    lcd.drawString("Katrij: Kad SD R35S (/nes) 35+ Game", 82, 63);
    lcd.setTextColor(TFT_SILVER, lcd.color565(15, 23, 42));
    lcd.drawString("Kawalan: On-Screen Touch D-Pad + A/B", 82, 77);
    lcd.setTextColor(TFT_SKYBLUE, lcd.color565(15, 23, 42));
    lcd.drawString(">> SENTUH UNTUK BUKA KATALOG NES <<", 82, 91);

    // 3. Kad Pilihan 2: ULAR & TANGGA (y=114..180, x=16..304)
    lcd.fillRoundRect(16, 114, 288, 66, 8, lcd.color565(15, 23, 42));
    lcd.drawRoundRect(16, 114, 288, 66, 8, lcd.color565(16, 185, 129)); // Emerald border

    // Badge Dadu
    lcd.fillRoundRect(24, 122, 50, 50, 6, lcd.color565(16, 120, 60)); // Hijau Dice Box
    lcd.drawRoundRect(24, 122, 50, 50, 6, TFT_WHITE);
    lcd.setTextColor(TFT_WHITE, lcd.color565(16, 120, 60));
    lcd.setTextSize(2);
    lcd.drawCenterString("DADU", 49, 130);
    lcd.setTextSize(1);
    lcd.drawCenterString("1P-4P", 49, 152);

    // Teks Kad 2
    lcd.setTextColor(TFT_WHITE, lcd.color565(15, 23, 42));
    lcd.setTextSize(1);
    lcd.drawString("2. ULAR & TANGGA (CRYPTO)", 82, 122);
    lcd.setTextColor(TFT_YELLOW, lcd.color565(15, 23, 42));
    lcd.drawString("Mod: 1 Hingga 4 Pemain + Bot Satoshi", 82, 137);
    lcd.setTextColor(TFT_SILVER, lcd.color565(15, 23, 42));
    lcd.drawString("Grafik 60 FPS + Papan Interaktif Touch", 82, 151);
    lcd.setTextColor(TFT_GREENYELLOW, lcd.color565(15, 23, 42));
    lcd.drawString(">> SENTUH UNTUK MAIN ULAR <<", 82, 165);

    // 4. Kad Pilihan 3: Kembali ke Mining Dashboard (y=188..230, x=16..304)
    lcd.fillRoundRect(16, 188, 288, 42, 6, lcd.color565(30, 41, 59));
    lcd.drawRoundRect(16, 188, 288, 42, 6, lcd.color565(71, 85, 105));
    lcd.setTextColor(TFT_GOLD, lcd.color565(30, 41, 59));
    lcd.drawCenterString("[ ⛏️ STATISTIK MINING DASHBOARD ]", lcd.width() / 2, 194);
    lcd.setTextColor(TFT_SILVER, lcd.color565(30, 41, 59));
    lcd.drawCenterString("Pantau Hashrate, WiFi, Valid Shares, Pool PPLNS", lcd.width() / 2, 210);

    lcd.endWrite();
}

static void drawNesGameSelector(const MinerStats& stats) {
    lcd.startWrite();
    lcd.fillScreen(lcd.color565(10, 15, 29)); // Midnight Dark

    int totalGames = s_cachedGames.size();
    int totalPages = (totalGames + NES_ITEMS_PER_PAGE - 1) / NES_ITEMS_PER_PAGE;
    if (totalPages == 0) totalPages = 1;
    if (s_nesPage >= totalPages) s_nesPage = totalPages - 1;
    if (s_nesPage < 0) s_nesPage = 0;

    // 1. Header Bar (y=0..32)
    lcd.fillRect(0, 0, lcd.width(), 32, lcd.color565(24, 34, 58));
    lcd.drawFastHLine(0, 32, lcd.width(), lcd.color565(56, 189, 248)); // Cyan border
    lcd.setTextColor(TFT_GOLD, lcd.color565(24, 34, 58));
    lcd.setTextSize(1);
    lcd.drawCenterString("=== KATALOG GAME NES (KAD SD) ===", lcd.width() / 2, 5);

    char subHdr[64];
    String fld = getSdDetectedFolder();
    if (fld.length() == 0) fld = "/nes";
    snprintf(subHdr, sizeof(subHdr), "Folder: %s | Mining: %.1fkH/s | Hal: %d/%d (%d Game)",
             fld.c_str(), stats.currentHashrate, s_nesPage + 1, totalPages, totalGames);
    lcd.setTextColor(TFT_CYAN, lcd.color565(24, 34, 58));
    lcd.drawCenterString(subHdr, lcd.width() / 2, 18);

    if (totalGames == 0) {
        // Tiada game dikesan atau kad SD belum dimasukkan
        lcd.fillRoundRect(16, 46, 288, 70, 8, lcd.color565(15, 23, 42));
        lcd.drawRoundRect(16, 46, 288, 70, 8, lcd.color565(234, 88, 12)); // Orange
        lcd.setTextColor(TFT_GOLD, lcd.color565(15, 23, 42));
        lcd.drawCenterString("⚠️ KAD MICROSD BELUM DIKESAN / TIADA GAME", lcd.width() / 2, 55);
        lcd.setTextColor(TFT_SILVER, lcd.color565(15, 23, 42));
        lcd.drawCenterString("Sila masukkan kad MicroSD R35S anda di slot CYD.", lcd.width() / 2, 73);
        lcd.setTextColor(TFT_YELLOW, lcd.color565(15, 23, 42));
        lcd.drawCenterString("Atau mainkan Game Demo Retro 8-bit terbina dalam:", lcd.width() / 2, 92);

        // Butang 1: Demo Retro
        lcd.fillRoundRect(24, 126, 272, 34, 6, lcd.color565(225, 29, 72));
        lcd.drawRoundRect(24, 126, 272, 34, 6, TFT_WHITE);
        lcd.setTextColor(TFT_WHITE, lcd.color565(225, 29, 72));
        lcd.drawCenterString("🎮 MAIN RETRO DEMO (SPACE SHOOTER)", lcd.width() / 2, 137);

        // Butang 2: Imbas Semula
        lcd.fillRoundRect(24, 168, 272, 30, 6, lcd.color565(5, 150, 105));
        lcd.drawRoundRect(24, 168, 272, 30, 6, TFT_WHITE);
        lcd.setTextColor(TFT_WHITE, lcd.color565(5, 150, 105));
        lcd.drawCenterString("🔄 IMBAS SEMULA KAD SD SEKARANG", lcd.width() / 2, 177);
    } else {
        // Paparkan 4 kad game untuk halaman semasa
        int startIdx = s_nesPage * NES_ITEMS_PER_PAGE;
        for (int i = 0; i < NES_ITEMS_PER_PAGE; i++) {
            int idx = startIdx + i;
            int cardY = 38 + i * 39;
            if (idx < totalGames) {
                const SdGameItem& item = s_cachedGames[idx];
                uint16_t bdrColor = item.isCompatible ? lcd.color565(16, 185, 129) : lcd.color565(202, 138, 4);
                
                // Kad Game
                lcd.fillRoundRect(12, cardY, 296, 36, 6, lcd.color565(15, 23, 42));
                lcd.drawRoundRect(12, cardY, 296, 36, 6, bdrColor);

                // Badge NES (Kiri)
                uint16_t badgeBg = item.isCompatible ? lcd.color565(5, 150, 105) : lcd.color565(180, 83, 9);
                lcd.fillRoundRect(16, cardY + 4, 34, 28, 4, badgeBg);
                lcd.setTextColor(TFT_WHITE, badgeBg);
                lcd.setTextSize(1);
                lcd.drawCenterString("NES", 16 + 17, cardY + 7);
                char szStr[12];
                snprintf(szStr, sizeof(szStr), "%uk", item.sizeKB);
                lcd.drawCenterString(szStr, 16 + 17, cardY + 18);

                // Nama Game (Tengah)
                lcd.setTextColor(TFT_WHITE, lcd.color565(15, 23, 42));
                String nameDisp = item.displayName;
                if (nameDisp.length() > 24) nameDisp = nameDisp.substring(0, 22) + "..";
                lcd.drawString(nameDisp, 56, cardY + 5);

                // Sub-info: Saiz & Keserasian
                if (item.isCompatible) {
                    lcd.setTextColor(TFT_GREENYELLOW, lcd.color565(15, 23, 42));
                    lcd.drawString("Disokong Penuh | NROM/Mapper 0", 56, cardY + 19);
                } else {
                    lcd.setTextColor(TFT_ORANGE, lcd.color565(15, 23, 42));
                    lcd.drawString("Saiz > 64KB | RAM Terhad", 56, cardY + 19);
                }

                // Butang MAIN (Kanan)
                uint16_t playBtnBg = item.isCompatible ? lcd.color565(37, 99, 235) : lcd.color565(71, 85, 105);
                lcd.fillRoundRect(250, cardY + 5, 52, 26, 4, playBtnBg);
                lcd.setTextColor(TFT_WHITE, playBtnBg);
                lcd.drawCenterString(item.isCompatible ? "MAIN" : "CUBA", 250 + 26, cardY + 13);
            }
        }
    }

    // 3. Bottom Bar Navigasi (y=196..236)
    // [ ◀ PREV ] (x=12..88)
    lcd.fillRoundRect(12, 198, 76, 36, 6, lcd.color565(30, 41, 59));
    lcd.drawRoundRect(12, 198, 76, 36, 6, lcd.color565(56, 189, 248));
    lcd.setTextColor(s_nesPage > 0 ? TFT_WHITE : TFT_DARKGREY, lcd.color565(30, 41, 59));
    lcd.drawCenterString("◀ PREV", 12 + 38, 210);

    // [ ⬅ GAME HUB ] (x=96..224)
    lcd.fillRoundRect(96, 198, 128, 36, 6, lcd.color565(225, 29, 72));
    lcd.drawRoundRect(96, 198, 128, 36, 6, TFT_WHITE);
    lcd.setTextColor(TFT_WHITE, lcd.color565(225, 29, 72));
    lcd.drawCenterString("⬅ GAME HUB", 96 + 64, 210);

    // [ NEXT ▶ ] (x=232..308)
    lcd.fillRoundRect(232, 198, 76, 36, 6, lcd.color565(30, 41, 59));
    lcd.drawRoundRect(232, 198, 76, 36, 6, lcd.color565(56, 189, 248));
    lcd.setTextColor((s_nesPage + 1 < totalPages) ? TFT_WHITE : TFT_DARKGREY, lcd.color565(30, 41, 59));
    lcd.drawCenterString("NEXT ▶", 232 + 38, 210);

    lcd.endWrite();
}

void gameTaskLoop(void* parameter) {
    Serial.println("[CORE 1] GameTask dimulakan: Ular & Tangga Touch Edition di Core 1");

    // Inisialisasi Paparan LCD LovyanGFX
    lcd.init();
    lcd.setRotation(currentRotation); // Landscape 320x240 (Rotasi 3)
    lcd.setBrightness(220);
    lcd.fillScreen(TFT_BLACK);

    // Inisialisasi NoFrendo Bridge perkakasan CYD
    initNofrendoBridge(&lcd);

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

        // 0. Semak jika terdapat arahan pelancaran ROM daripada WebGUI
        String webRom;
        if (checkPendingRomRequest(webRom)) {
            Serial.printf("[GAME TASK] Permintaan main ROM dari WebGUI: '%s'\n", webRom.c_str());
            int lastSlash = webRom.lastIndexOf('/');
            if (lastSlash >= 0) webRom = webRom.substring(lastSlash + 1);

            String fullPath;
            if (webRom.equalsIgnoreCase("Ice Climber.nes") || webRom.equalsIgnoreCase("Ice Climber") || webRom == "builtin") {
                fullPath = "Ice Climber.nes";
            } else {
                String folder = getSdDetectedFolder();
                if (!folder.startsWith("/")) folder = "/" + folder;
                if (folder.endsWith("/")) folder = folder.substring(0, folder.length() - 1);
                fullPath = "/sd" + folder + "/" + webRom;
            }

            currentMode = MODE_GAME_NES;
            s_isRealRomPlaying = true;

            // Paparkan skrin memuatkan dari WebGUI
            lcd.fillScreen(TFT_BLACK);
            lcd.setTextColor(TFT_GOLD, TFT_BLACK);
            lcd.setTextSize(1);
            lcd.drawCenterString("=== MEMUATKAN DARI WEBGUI ===", lcd.width() / 2, 70);
            lcd.setTextColor(TFT_WHITE, TFT_BLACK);
            lcd.setTextSize(2);
            String title = webRom;
            if (title.endsWith(".nes") || title.endsWith(".NES")) title = title.substring(0, title.length() - 4);
            if (title.length() > 18) title = title.substring(0, 18);
            lcd.drawCenterString(title.c_str(), lcd.width() / 2, 95);
            lcd.setTextSize(1);
            lcd.setTextColor(TFT_GREEN, TFT_BLACK);
            lcd.drawCenterString("Enjin: NoFrendo Zero-Copy 60FPS", lcd.width() / 2, 125);
            lcd.setTextColor(TFT_SKYBLUE, TFT_BLACK);
            lcd.drawCenterString("Background Solo/Pool Mining Aktif...", lcd.width() / 2, 145);
            vTaskDelay(pdMS_TO_TICKS(400));

            // Bebaskan sprite untuk pulihkan 47KB RAM
            hudSprite.deleteSprite();
            panelSprite.deleteSprite();

            int res = runNofrendoGame(fullPath.c_str());

            // Cipta semula sprite selepas sesi selesai
            hudSprite.setColorDepth(16);
            hudSprite.createSprite(320, 24);
            panelSprite.setColorDepth(16);
            panelSprite.createSprite(78, 204);

            if (res != 0) {
                lcd.fillScreen(TFT_BLACK);
                lcd.fillRoundRect(16, 45, 288, 150, 8, lcd.color565(30, 41, 59));
                lcd.drawRoundRect(16, 45, 288, 150, 8, lcd.color565(234, 88, 12));
                lcd.setTextColor(TFT_GOLD, lcd.color565(30, 41, 59));
                lcd.drawCenterString("=== HAD MEMORI SRAM ESP32 ===", 160, 60);
                lcd.setTextColor(TFT_WHITE, lcd.color565(30, 41, 59));
                char errBuf[64];
                snprintf(errBuf, sizeof(errBuf), "Ralat muat ROM '%s' (Kod: %d)", webRom.c_str(), res);
                lcd.drawCenterString(errBuf, 160, 85);
                vTaskDelay(pdMS_TO_TICKS(2500));
            }

            currentMode = MODE_NES_SELECT;
            s_isRealRomPlaying = false;
            modeSwitched = true;
            continue;
        }

        int32_t touchX = 0, touchY = 0;
        bool touched = lcd.getTouch(&touchX, &touchY);
        if (touched && (touchX < 0 || touchX >= 320 || touchY < 0 || touchY >= 240)) {
            touched = false;
        }
        bool btnBoot = (digitalRead(PIN_BTN_LEFT) == LOW);

        if (touched || btnBoot) {
            lastUserInputTime = millis();

            if (currentMode == MODE_GAME_MENU && touched) {
                // Sentuhan pada 3 Kad Pilihan di Game Hub
                // 1. Kad RETRO NES (y=40..106) -> Buka Katalog Game Kad SD
                if (touchY >= 40 && touchY <= 106) {
                    currentMode = MODE_NES_SELECT;
                    s_nesPage = 0;
                    s_cachedGames = getSdGameList(false);
                    modeSwitched = true;
                    int32_t dx, dy;
                    while (lcd.getTouch(&dx, &dy)) {
                        vTaskDelay(pdMS_TO_TICKS(50));
                    }
                    vTaskDelay(pdMS_TO_TICKS(150));
                    continue;
                }
                // 2. Kad ULAR & TANGGA (y=114..180)
                else if (touchY >= 114 && touchY <= 180) {
                    currentMode = MODE_GAME_SNAKES;
                    game.needBoardRedraw = true;
                    game.needPanelRedraw = true;
                    modeSwitched = true;
                    vTaskDelay(pdMS_TO_TICKS(250));
                    continue;
                }
                // 3. Kad MINING DASHBOARD (y=188..236)
                else if (touchY >= 188 && touchY <= 236) {
                    currentMode = MODE_MINER_DASHBOARD;
                    modeSwitched = true;
                    vTaskDelay(pdMS_TO_TICKS(250));
                    continue;
                }
            } else if (currentMode == MODE_NES_SELECT && touched) {
                // Sentuhan di Katalog Game NES Kad SD
                int totalGames = s_cachedGames.size();
                int totalPages = (totalGames + NES_ITEMS_PER_PAGE - 1) / NES_ITEMS_PER_PAGE;
                if (totalPages == 0) totalPages = 1;

                // 1. Navigasi Bawah (y >= 195)
                if (touchY >= 195) {
                    // [ ◀ PREV ] (x < 90)
                    if (touchX < 90) {
                        if (s_nesPage > 0) {
                            s_nesPage--;
                            modeSwitched = true;
                        }
                        vTaskDelay(pdMS_TO_TICKS(200));
                        continue;
                    }
                    // [ ⬅ GAME HUB ] (x >= 95 && x <= 225)
                    else if (touchX >= 95 && touchX <= 225) {
                        currentMode = MODE_GAME_MENU;
                        modeSwitched = true;
                        vTaskDelay(pdMS_TO_TICKS(250));
                        continue;
                    }
                    // [ NEXT ▶ ] (x > 230)
                    else if (touchX > 230) {
                        if (s_nesPage + 1 < totalPages) {
                            s_nesPage++;
                            modeSwitched = true;
                        }
                        vTaskDelay(pdMS_TO_TICKS(200));
                        continue;
                    }
                }

                // 2. Mod Kad Kosong / Belum Imbas
                if (totalGames == 0) {
                    // Butang 1: Demo Retro (y=120..165)
                    if (touchY >= 120 && touchY <= 165) {
                        currentMode = MODE_GAME_NES;
                        s_isRealRomPlaying = false;
                        nesGame.reset();
                        modeSwitched = true;
                        vTaskDelay(pdMS_TO_TICKS(250));
                        continue;
                    }
                    // Butang 2: Imbas Semula (y=166..200)
                    if (touchY >= 166 && touchY <= 200) {
                        s_cachedGames = getSdGameList(true);
                        modeSwitched = true;
                        vTaskDelay(pdMS_TO_TICKS(350));
                        continue;
                    }
                } else {
                    // 3. Sentuhan pada 4 Kad Game (y=38..192)
                    for (int i = 0; i < NES_ITEMS_PER_PAGE; i++) {
                        int cardY = 38 + i * 39;
                        if (touchY >= cardY && touchY < cardY + 36) {
                            int idx = s_nesPage * NES_ITEMS_PER_PAGE + i;
                            if (idx < totalGames) {
                                SdGameItem item = s_cachedGames[idx];

                                // Paparkan skrin memuatkan ROM
                                lcd.fillScreen(TFT_BLACK);
                                lcd.setTextColor(TFT_GOLD, TFT_BLACK);
                                lcd.setTextSize(1);
                                lcd.drawCenterString("=== MEMUATKAN KATRIJ NES ===", lcd.width() / 2, 70);
                                lcd.setTextColor(TFT_WHITE, TFT_BLACK);
                                lcd.setTextSize(2);
                                String shortTitle = item.displayName;
                                if (shortTitle.length() > 18) shortTitle = shortTitle.substring(0, 18);
                                lcd.drawCenterString(shortTitle.c_str(), lcd.width() / 2, 95);
                                lcd.setTextSize(1);
                                lcd.setTextColor(item.isCompatible ? TFT_GREEN : TFT_ORANGE, TFT_BLACK);
                                char infoSz[64];
                                snprintf(infoSz, sizeof(infoSz), "Saiz: %u KB | Enjin: NoFrendo 60FPS", item.sizeKB);
                                lcd.drawCenterString(infoSz, lcd.width() / 2, 125);
                                lcd.setTextColor(TFT_SKYBLUE, TFT_BLACK);
                                lcd.drawCenterString("Background Solo/Pool Mining Aktif...", lcd.width() / 2, 145);
                                vTaskDelay(pdMS_TO_TICKS(400));

                                // Format laluan fail SD dengan tepat
                                String fn = item.filename;
                                int lastSlash = fn.lastIndexOf('/');
                                if (lastSlash >= 0) fn = fn.substring(lastSlash + 1);

                                String fullPath;
                                if (fn.equalsIgnoreCase("Ice Climber.nes") || fn.equalsIgnoreCase("Ice Climber")) {
                                    fullPath = "Ice Climber.nes";
                                } else {
                                    String folder = getSdDetectedFolder();
                                    if (!folder.startsWith("/")) folder = "/" + folder;
                                    if (folder.endsWith("/")) folder = folder.substring(0, folder.length() - 1);
                                    fullPath = "/sd" + folder + "/" + fn;
                                }

                                currentMode = MODE_GAME_NES;
                                s_isRealRomPlaying = true;

                                // Pastikan sentuhan diangkat sebelum masuk ke emulator
                                int32_t rx, ry;
                                while (lcd.getTouch(&rx, &ry)) {
                                    vTaskDelay(pdMS_TO_TICKS(50));
                                }

                                // BEBASKAN 47KB MEMORI SPRITE (hudSprite 15KB + panelSprite 32KB)
                                // supaya RAM dalaman ESP32 mencukupi untuk framebuffer & NoFrendo
                                hudSprite.deleteSprite();
                                panelSprite.deleteSprite();
                                Serial.printf("[NES] Heap sedia sebelum NoFrendo: free=%u, maxAlloc=%u\n",
                                              ESP.getFreeHeap(), ESP.getMaxAllocHeap());

                                // Jalankan emulator NoFrendo
                                int res = runNofrendoGame(fullPath.c_str());

                                // CIPTA SEMULA SPRITE selepas sesi NoFrendo tamat
                                hudSprite.setColorDepth(16);
                                hudSprite.createSprite(320, 24);
                                panelSprite.setColorDepth(16);
                                panelSprite.createSprite(78, 204);

                                if (res != 0) {
                                    // Amaran jika ROM melebihi had RAM SRAM ESP32 (cth: ROM 256KB-1024KB)
                                    lcd.fillScreen(TFT_BLACK);
                                    lcd.fillRoundRect(16, 45, 288, 150, 8, lcd.color565(30, 41, 59));
                                    lcd.drawRoundRect(16, 45, 288, 150, 8, lcd.color565(234, 88, 12));
                                    lcd.setTextColor(TFT_GOLD, lcd.color565(30, 41, 59));
                                    lcd.setTextSize(1);
                                    lcd.drawCenterString("=== HAD MEMORI SRAM ESP32 ===", 160, 60);
                                    lcd.setTextColor(TFT_WHITE, lcd.color565(30, 41, 59));
                                    char errBuf[64];
                                    snprintf(errBuf, sizeof(errBuf), "Ralat muat ROM (Kod: %d | Free: %u B)", res, ESP.getFreeHeap());
                                    lcd.drawCenterString(errBuf, 160, 85);
                                    lcd.setTextColor(TFT_GREENYELLOW, lcd.color565(30, 41, 59));
                                    lcd.drawCenterString("Sila pilih game bertanda [ SIAP MAIN ] (<= 64KB)", 160, 110);
                                    lcd.setTextColor(TFT_CYAN, lcd.color565(30, 41, 59));
                                    lcd.drawCenterString("Cth: Circus (24KB), Tank Wars (24KB), Tetris (64KB)", 160, 130);
                                    lcd.setTextColor(TFT_SILVER, lcd.color565(30, 41, 59));
                                    lcd.drawCenterString(">> Membuka semula Katalog NES... <<", 160, 160);
                                    vTaskDelay(pdMS_TO_TICKS(3500));
                                }

                                // Sesi tamat, kembali ke katalog
                                currentMode = MODE_NES_SELECT;
                                s_isRealRomPlaying = false;
                                modeSwitched = true;
                                vTaskDelay(pdMS_TO_TICKS(250));
                                break;
                            }
                        }
                    }
                }
            } else if (currentMode == MODE_GAME_SNAKES && touched) {
                // Sentuhan di bar HUD atas (touchY < 25)
                if (touchY < 25) {
                    // Butang Tukar Mod Solo <-> Pool (x=45..108)
                    if (touchX >= 45 && touchX <= 108) {
                        toggleMiningPoolMode();
                        drawMiniMiningHUD(g_minerData.getStats());
                        vTaskDelay(pdMS_TO_TICKS(350));
                        continue;
                    }
                    // Butang HUB (Kembali ke Game Hub: x > 265)
                    if (touchX > 265) {
                        currentMode = MODE_GAME_MENU;
                        modeSwitched = true;
                        vTaskDelay(pdMS_TO_TICKS(250));
                        continue;
                    }
                }

                // Kawasan permainan Ular & Tangga: baling dadu / mod pemain
                if (game.handleTouch(touchX, touchY)) {
                    vTaskDelay(pdMS_TO_TICKS(180));
                }
            } else if (currentMode == MODE_GAME_NES && !s_isRealRomPlaying) {
                // Kawalan Sentuh Butang Maya D-Pad & A/B di Skrin NES Demo
                bool exitToHub = nesGame.handleTouch(touchX, touchY, touched);
                if (exitToHub) {
                    currentMode = MODE_NES_SELECT;
                    modeSwitched = true;
                    vTaskDelay(pdMS_TO_TICKS(250));
                    continue;
                }
            } else if (currentMode == MODE_MINER_DASHBOARD && touched) {
                // Butang Tukar Mod Solo <-> Pool di Dashboard (y=170..214)
                if (touchY >= 170 && touchY <= 214) {
                    toggleMiningPoolMode();
                    drawMinerDashboard(g_minerData.getStats(), true);
                    vTaskDelay(pdMS_TO_TICKS(350));
                    continue;
                }

                // Sentuh di luar butang mod untuk kembali ke Game Hub
                currentMode = MODE_GAME_MENU;
                modeSwitched = true;
                vTaskDelay(pdMS_TO_TICKS(250));
                continue;
            }
        } else if (currentMode == MODE_GAME_NES && !s_isRealRomPlaying) {
            // Lepaskan butang maya apabila sentuhan diangkat pada demo
            nesGame.handleTouch(0, 0, false);
        }

        // Kendalian Butang BOOT fizikal (Tekan pendek = Aksi, Tekan > 1.2s = Putar Skrin)
        if (btnBoot) {
            if (bootPressStart == 0) bootPressStart = millis();
            if (millis() - bootPressStart > 1200) {
                currentRotation = (currentRotation == 3) ? 1 : 3;
                lcd.setRotation(currentRotation);
                lcd.fillScreen(TFT_BLACK);
                game.needBoardRedraw = true;
                game.needPanelRedraw = true;
                nesGame.needFullBezelRedraw = true;
                modeSwitched = true;
                bootPressStart = 0;
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
        } else {
            if (bootPressStart > 0 && millis() - bootPressStart < 1200) {
                if (currentMode == MODE_GAME_SNAKES && game.canRoll()) {
                    game.rollDice();
                } else if (currentMode == MODE_GAME_NES && !s_isRealRomPlaying) {
                    nesGame.fireLaser();
                } else if (currentMode == MODE_GAME_MENU) {
                    currentMode = MODE_NES_SELECT;
                    s_nesPage = 0;
                    s_cachedGames = getSdGameList(false);
                    modeSwitched = true;
                } else if (currentMode == MODE_NES_SELECT) {
                    currentMode = MODE_GAME_MENU;
                    modeSwitched = true;
                } else if (currentMode == MODE_MINER_DASHBOARD) {
                    currentMode = MODE_GAME_MENU;
                    modeSwitched = true;
                }
            }
            bootPressStart = 0;
        }

        // Auto Screensaver ke Mining Dashboard jika idle 25 saat
        if (currentMode != MODE_MINER_DASHBOARD && (millis() - lastUserInputTime > IDLE_TIMEOUT_MS)) {
            currentMode = MODE_MINER_DASHBOARD;
            modeSwitched = true;
        }

        // 2. Pemprosesan Render Mengikut Mod (Sifar Flicker)
        if (currentMode == MODE_GAME_MENU) {
            if (modeSwitched || (millis() - lastHudUpdateTime > 1000)) {
                drawGameHubMenu(stats);
                lastHudUpdateTime = millis();
                modeSwitched = false;
            }
            vTaskDelay(pdMS_TO_TICKS(30));
        } else if (currentMode == MODE_NES_SELECT) {
            if (modeSwitched || (millis() - lastHudUpdateTime > 1000)) {
                drawNesGameSelector(stats);
                lastHudUpdateTime = millis();
                modeSwitched = false;
            }
            vTaskDelay(pdMS_TO_TICKS(30));
        } else if (currentMode == MODE_GAME_SNAKES) {
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
        } else if (currentMode == MODE_GAME_NES) {
            if (!s_isRealRomPlaying) {
                if (modeSwitched) {
                    nesGame.needFullBezelRedraw = true;
                    modeSwitched = false;
                }
                nesGame.updateAndRender(lcd);
            }
            vTaskDelay(pdMS_TO_TICKS(16)); // ~60 FPS
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
