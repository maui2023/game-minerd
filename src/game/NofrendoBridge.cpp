#include "NofrendoBridge.h"
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <errno.h>

#include "../miner/MinerTask.h"
#include "BuiltinRom.h"

extern "C" {
#include <noftypes.h>
#include <nofrendo.h>
#include <osd.h>
#include <event.h>
#include <gui.h>
#include <log.h>
#include <nes/nes.h>
#include <nes/nesinput.h>
#include <nes/nes_rom.h>
#include <nofconfig.h>
}

static LGFX* g_pLcd = NULL;
static uint16_t g_myPalette[256];
static bitmap_t *g_myBitmap = NULL;
static esp_timer_handle_t g_nesTimer = NULL;
static void (*g_timerFunc)(void) = NULL;
static unsigned long g_bootPressStart = 0;

void initNofrendoBridge(LGFX* pLcd) {
    g_pLcd = pLcd;
}

static void IRAM_ATTR nesTimerCallback(void* arg) {
    if (g_timerFunc) {
        g_timerFunc();
    }
}

static void nes_timer_cleanup() {
    if (g_nesTimer != NULL) {
        esp_timer_stop(g_nesTimer);
        esp_timer_delete(g_nesTimer);
        g_nesTimer = NULL;
    }
    g_timerFunc = NULL;
}

// -------------------------------------------------------------
// Peruntukan Memori (Heap)
// -------------------------------------------------------------
extern "C" void *mem_alloc(int size, bool prefer_fast_memory) {
    (void)prefer_fast_memory;
    void *ptr = malloc(size);
    if (!ptr) {
        ptr = heap_caps_malloc(size, MALLOC_CAP_8BIT);
    }
    return ptr;
}

// -------------------------------------------------------------
// Video Driver (LovyanGFX DMA / FIFO Stream)
// -------------------------------------------------------------
static int vid_init_func(int width, int height) {
    return 0;
}

static void vid_shutdown_func(void) {
    if (g_myBitmap) {
        bmp_destroy(&g_myBitmap);
        g_myBitmap = NULL;
    }
}

static int vid_set_mode_func(int width, int height) {
    return 0;
}

static void vid_set_palette_func(rgb_t *pal) {
    if (!g_pLcd) return;
    for (int i = 0; i < 256; i++) {
        g_myPalette[i] = g_pLcd->color565(pal[i].r, pal[i].g, pal[i].b);
    }
}

static void vid_clear_func(uint8 color) {
    if (g_pLcd) {
        g_pLcd->fillRect(32, 0, 256, 240, TFT_BLACK);
    }
}

static bitmap_t *vid_lock_write_func(void) {
    if (!g_myBitmap) {
        static uint8_t s_dummyFb[512];
        g_myBitmap = bmp_createhw(s_dummyFb, NES_SCREEN_WIDTH, NES_SCREEN_HEIGHT, NES_SCREEN_WIDTH);
    }
    return g_myBitmap;
}

static void vid_free_write_func(int num_dirties, rect_t *dirty_rects) {
    // Kekalkan g_myBitmap sepanjang sesi supaya screen->width/height dalam vid_drv sentiasa sah
}

static uint32_t s_lastControllerVal = 0xFFFFFFFF;

// -------------------------------------------------------------
// Lukis Overlay Kawalan Sentuh Pada Permainan NES
// (D-Pad Gaya Cincin + Panah ke Dalam, Butang A/B Besar 42px)
// -------------------------------------------------------------
void drawNofrendoOverlay(uint32_t activeInput) {
    if (!g_pLcd) return;
    g_pLcd->startWrite();

    bool active_up   = !(activeInput & (1 << 0));
    bool active_down = !(activeInput & (1 << 1));
    bool active_left = !(activeInput & (1 << 2));
    bool active_rgt  = !(activeInput & (1 << 3));
    bool active_sel  = !(activeInput & (1 << 4));
    bool active_sta  = !(activeInput & (1 << 5));
    bool active_a    = !(activeInput & (1 << 6));
    bool active_b    = !(activeInput & (1 << 7));

    // --- 1. D-PAD BARU (Cincin Bulat + 4 Butang Panah Menghala Ke Tengah Sesuai Gambar Rujukan) ---
    const int32_t cx = 50;
    const int32_t cy = 168;

    // Cincin Luar (Menyambungkan 4 butang arah seperti gambar rujukan pengguna)
    uint16_t ringCol = g_pLcd->color565(148, 163, 184); // Slate 400
    g_pLcd->drawCircle(cx, cy, 28, ringCol);
    g_pLcd->drawCircle(cx, cy, 29, ringCol);

    auto drawPad = [&](bool active, int dir) {
        uint16_t fCol = active ? g_pLcd->color565(250, 204, 21) : g_pLcd->color565(20, 25, 35);
        uint16_t bCol = active ? TFT_WHITE : g_pLcd->color565(226, 232, 240);
        uint16_t iCol = active ? g_pLcd->color565(15, 23, 42) : g_pLcd->color565(203, 213, 225);

        if (dir == 0) { // ATAS (Panah Menghala Ke Bawah/Pusat)
            g_pLcd->fillRoundRect(cx - 10, cy - 38, 21, 20, 3, fCol);
            g_pLcd->fillTriangle(cx - 10, cy - 19, cx + 10, cy - 19, cx, cy - 9, fCol);
            g_pLcd->drawFastHLine(cx - 8, cy - 38, 17, bCol);
            g_pLcd->drawFastVLine(cx - 10, cy - 36, 17, bCol);
            g_pLcd->drawFastVLine(cx + 10, cy - 36, 17, bCol);
            g_pLcd->drawLine(cx - 10, cy - 19, cx, cy - 9, bCol);
            g_pLcd->drawLine(cx + 10, cy - 19, cx, cy - 9, bCol);
            g_pLcd->fillTriangle(cx - 4, cy - 27, cx + 4, cy - 27, cx, cy - 21, iCol);
        } else if (dir == 1) { // BAWAH (Panah Menghala Ke Atas/Pusat)
            g_pLcd->fillRoundRect(cx - 10, cy + 19, 21, 20, 3, fCol);
            g_pLcd->fillTriangle(cx - 10, cy + 19, cx + 10, cy + 19, cx, cy + 9, fCol);
            g_pLcd->drawFastHLine(cx - 8, cy + 38, 17, bCol);
            g_pLcd->drawFastVLine(cx - 10, cy + 19, 17, bCol);
            g_pLcd->drawFastVLine(cx + 10, cy + 19, 17, bCol);
            g_pLcd->drawLine(cx - 10, cy + 19, cx, cy + 9, bCol);
            g_pLcd->drawLine(cx + 10, cy + 19, cx, cy + 9, bCol);
            g_pLcd->fillTriangle(cx - 4, cy + 27, cx + 4, cy + 27, cx, cy + 21, iCol);
        } else if (dir == 2) { // KIRI (Panah Menghala Ke Kanan/Pusat)
            g_pLcd->fillRoundRect(cx - 38, cy - 10, 20, 21, 3, fCol);
            g_pLcd->fillTriangle(cx - 19, cy - 10, cx - 19, cy + 10, cx - 9, cy, fCol);
            g_pLcd->drawFastVLine(cx - 38, cy - 8, 17, bCol);
            g_pLcd->drawFastHLine(cx - 36, cy - 10, 17, bCol);
            g_pLcd->drawFastHLine(cx - 36, cy + 10, 17, bCol);
            g_pLcd->drawLine(cx - 19, cy - 10, cx - 9, cy, bCol);
            g_pLcd->drawLine(cx - 19, cy + 10, cx - 9, cy, bCol);
            g_pLcd->fillTriangle(cx - 27, cy - 4, cx - 27, cy + 4, cx - 21, cy, iCol);
        } else if (dir == 3) { // KANAN (Panah Menghala Ke Kiri/Pusat)
            g_pLcd->fillRoundRect(cx + 19, cy - 10, 20, 21, 3, fCol);
            g_pLcd->fillTriangle(cx + 19, cy - 10, cx + 19, cy + 10, cx + 9, cy, fCol);
            g_pLcd->drawFastVLine(cx + 38, cy - 8, 17, bCol);
            g_pLcd->drawFastHLine(cx + 19, cy - 10, 17, bCol);
            g_pLcd->drawFastHLine(cx + 19, cy + 10, 17, bCol);
            g_pLcd->drawLine(cx + 19, cy - 10, cx + 9, cy, bCol);
            g_pLcd->drawLine(cx + 19, cy + 10, cx + 9, cy, bCol);
            g_pLcd->fillTriangle(cx + 27, cy - 4, cx + 27, cy + 4, cx + 21, cy, iCol);
        }
    };

    drawPad(active_up, 0);
    drawPad(active_down, 1);
    drawPad(active_left, 2);
    drawPad(active_rgt, 3);

    // --- 2. BUTANG AKSI B & A (DIBESARKAN: Diameter 42px untuk Mudah Ditekan) ---
    const int32_t bx = 244, by = 182;
    const int32_t ax = 292, ay = 134;

    // Butang B (Merah Ruby)
    uint16_t fill_b   = active_b ? g_pLcd->color565(239, 68, 68) : g_pLcd->color565(127, 29, 29);
    uint16_t border_b = active_b ? TFT_WHITE : g_pLcd->color565(251, 113, 133);
    g_pLcd->fillCircle(bx, by, 21, fill_b);
    g_pLcd->drawCircle(bx, by, 21, border_b);
    g_pLcd->drawCircle(bx, by, 20, border_b);
    g_pLcd->setTextSize(2);
    g_pLcd->setTextColor(TFT_WHITE, fill_b);
    g_pLcd->drawCenterString("B", bx, by - 7);

    // Butang A (Biru Azure)
    uint16_t fill_a   = active_a ? g_pLcd->color565(56, 189, 248) : g_pLcd->color565(30, 58, 138);
    uint16_t border_a = active_a ? TFT_WHITE : g_pLcd->color565(96, 165, 250);
    g_pLcd->fillCircle(ax, ay, 21, fill_a);
    g_pLcd->drawCircle(ax, ay, 21, border_a);
    g_pLcd->drawCircle(ax, ay, 20, border_a);
    g_pLcd->setTextSize(2);
    g_pLcd->setTextColor(TFT_WHITE, fill_a);
    g_pLcd->drawCenterString("A", ax, ay - 7);

    // --- 3. BAR ATAS: HUB, SELECT, START ---
    // Butang Keluar HUB (Merah)
    g_pLcd->fillRoundRect(272, 4, 44, 20, 4, g_pLcd->color565(185, 28, 28));
    g_pLcd->drawRoundRect(272, 4, 44, 20, 4, TFT_WHITE);
    g_pLcd->setTextSize(1);
    g_pLcd->setTextColor(TFT_WHITE, g_pLcd->color565(185, 28, 28));
    g_pLcd->drawCenterString("HUB", 294, 10);

    // SELECT
    uint16_t fill_sel   = active_sel ? g_pLcd->color565(52, 211, 153) : g_pLcd->color565(20, 25, 35);
    uint16_t border_sel = active_sel ? TFT_WHITE : g_pLcd->color565(148, 163, 184);
    uint16_t text_sel   = active_sel ? TFT_BLACK : TFT_WHITE;
    g_pLcd->fillRoundRect(106, 5, 48, 18, 4, fill_sel);
    g_pLcd->drawRoundRect(106, 5, 48, 18, 4, border_sel);
    g_pLcd->setTextColor(text_sel, fill_sel);
    g_pLcd->drawCenterString("SELECT", 130, 10);

    // START
    uint16_t fill_sta   = active_sta ? g_pLcd->color565(52, 211, 153) : g_pLcd->color565(20, 25, 35);
    uint16_t border_sta = active_sta ? TFT_WHITE : g_pLcd->color565(148, 163, 184);
    uint16_t text_sta   = active_sta ? TFT_BLACK : TFT_WHITE;
    g_pLcd->fillRoundRect(164, 5, 48, 18, 4, fill_sta);
    g_pLcd->drawRoundRect(164, 5, 48, 18, 4, border_sta);
    g_pLcd->setTextColor(text_sta, fill_sta);
    g_pLcd->drawCenterString("START", 188, 10);

    g_pLcd->endWrite();
}

void drawNofrendoBezel() {
    drawNofrendoOverlay(s_lastControllerVal);
}

static void vid_custom_blit_func(bitmap_t *bmp, int num_dirties, rect_t *dirty_rects) {
    if (!g_pLcd || !bmp || !bmp->line) return;

    static uint16_t lineBuf[256];
    g_pLcd->startWrite();
    g_pLcd->setAddrWindow(32, 0, 256, 240);
    for (int y = 0; y < 240; y++) {
        const uint8_t *row = bmp->line[y];
        for (int x = 0; x < 256; x++) {
            lineBuf[x] = g_myPalette[row[x]];
        }
        g_pLcd->writePixels(lineBuf, 256);
    }

    // Lukis overlay kawalan maya terus di atas lapisan permainan tanpa kerlipan
    drawNofrendoOverlay(s_lastControllerVal);

    g_pLcd->endWrite();
}

static viddriver_t g_cydDriver = {
    "CYD LovyanGFX Driver",
    vid_init_func,
    vid_shutdown_func,
    vid_set_mode_func,
    vid_set_palette_func,
    vid_clear_func,
    vid_lock_write_func,
    vid_free_write_func,
    vid_custom_blit_func,
    false
};

extern "C" void osd_getvideoinfo(vidinfo_t *info) {
    info->default_width = NES_SCREEN_WIDTH;
    info->default_height = NES_SCREEN_HEIGHT;
    info->driver = &g_cydDriver;
}

// -------------------------------------------------------------
// Kawalan Sentuh & Input Joypad NES
// -------------------------------------------------------------
static uint32_t cyd_controller_read() {
    uint32_t val = 0xFFFFFFFF; // 1 = dilepaskan, 0 = ditekan (active low)
    if (!g_pLcd) return val;

    int32_t tx = -1, ty = -1;
    bool touched = g_pLcd->getTouch(&tx, &ty);
    bool btnBoot = (digitalRead(0) == LOW);

    // Butang fizikal BOOT:
    // Tekan pendek = Butang A (Lompat / Tembak)
    // Tekan lama (> 1.5 saat) = Keluar kembali ke Katalog NES
    if (btnBoot) {
        if (g_bootPressStart == 0) g_bootPressStart = millis();
        if (millis() - g_bootPressStart > 1500) {
            main_quit();
            return val;
        } else {
            val ^= (1 << 6); // Butang A
        }
    } else {
        g_bootPressStart = 0;
    }

    if (touched && tx >= 0 && tx < 320 && ty >= 0 && ty < 240) {
        // 1. Sentuhan pada Butang Keluar [HUB] (Top-Right: tx >= 265 && ty <= 32)
        if (tx >= 265 && ty <= 32) {
            main_quit();
            return val;
        }

        // 2. Bar Atas: SELECT & START (ty <= 35)
        if (ty <= 35) {
            if (tx >= 95 && tx < 155) {
                val ^= (1 << 4); // SELECT
            } else if (tx >= 155 && tx <= 220) {
                val ^= (1 << 5); // START
            }
        }

        // 3. Butang Aksi Besar A & B (Kawasan Jemari Kanan: tx >= 210 && ty >= 70)
        int32_t db2 = (tx - 244) * (tx - 244) + (ty - 182) * (ty - 182);
        int32_t da2 = (tx - 292) * (tx - 292) + (ty - 134) * (ty - 134);

        if (db2 <= 1156) { // radius <= 34px
            val ^= (1 << 7); // Butang B
        }
        if (da2 <= 1156) { // radius <= 34px
            val ^= (1 << 6); // Butang A
        }
        // Kawasan toleransi luas sekiranya ditekan sedikit di luar jejari bulatan
        if (tx >= 210 && ty >= 70 && !(db2 <= 1156) && !(da2 <= 1156)) {
            if (db2 < da2 && db2 <= 2304) { // radius <= 48px
                val ^= (1 << 7); // Butang B
            } else if (da2 < db2 && da2 <= 2304) { // radius <= 48px
                val ^= (1 << 6); // Butang A
            }
        }

        // 4. Pad Arah Maya Cincin + Panah (Pusat di cx=50, cy=168)
        int32_t dx = tx - 50;
        int32_t dy = ty - 168;
        int32_t dist2 = dx * dx + dy * dy;

        // Zon sentuhan jemari kiri: jejari hingga 55px atau zon kiri bawah (tx <= 115, ty >= 85, dist <= 75px)
        if (dist2 <= 3025 || (tx <= 115 && ty >= 85 && dist2 <= 5625)) {
            // Zon mati 7px di tengah untuk elak sentuhan tidak sengaja
            if (dist2 > 49) {
                if (dy < -9) val ^= (1 << 0); // ATAS
                if (dy > 9)  val ^= (1 << 1); // BAWAH
                if (dx < -9) val ^= (1 << 2); // KIRI
                if (dx > 9)  val ^= (1 << 3); // KANAN
            }
        }
    }

    s_lastControllerVal = val;
    return val;
}

extern "C" void osd_getinput(void) {
    const int ev[32] = {
        event_joypad1_up, event_joypad1_down, event_joypad1_left, event_joypad1_right,
        event_joypad1_select, event_joypad1_start, event_joypad1_a, event_joypad1_b,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0
    };
    static uint32_t oldb = 0xFFFFFFFF;
    uint32_t b = cyd_controller_read();
    uint32_t chg = b ^ oldb;
    oldb = b;
    for (int x = 0; x < 8; x++) {
        if (chg & (1 << x)) {
            event_t evh = event_get(ev[x]);
            if (evh) {
                evh((b & (1 << x)) ? INP_STATE_BREAK : INP_STATE_MAKE);
            }
        }
    }
    // Rehat seketika untuk membolehkan pengaturcaraan FreeRTOS bernafas
    vTaskDelay(pdMS_TO_TICKS(1));
}


// -------------------------------------------------------------
// Audio / Bunyi (Dummy Stubs untuk Menjimatkan CPU & RAM)
// -------------------------------------------------------------
extern "C" void osd_getsoundinfo(sndinfo_t *info) {
    if (info) {
        info->sample_rate = 0;
        info->bps = 0;
    }
}

extern "C" void osd_setsound(void (*playfunc)(void *buffer, int size)) {
    (void)playfunc;
}

// -------------------------------------------------------------
// Pengurusan Fail & Snapshots
// -------------------------------------------------------------
extern "C" void osd_fullname(char *fullname, const char *shortname) {
    strncpy(fullname, shortname, PATH_MAX);
}

extern "C" char *osd_newextension(char *string, char *ext) {
    size_t l = strlen(string);
    if (l > 3) {
        string[l - 3] = ext[1];
        string[l - 2] = ext[2];
        string[l - 1] = ext[3];
    }
    return string;
}

extern "C" int osd_makesnapname(char *filename, int len) {
    return -1;
}

extern "C" void osd_getmouse(int *x, int *y, int *button) {
    *x = 0; *y = 0; *button = 0;
}

// -------------------------------------------------------------
// OSD Timer, Init, Shutdown & Main
// -------------------------------------------------------------
extern "C" int osd_installtimer(int frequency, void *func, int funcsize, void *counter, int countersize) {
    nes_timer_cleanup();
    g_timerFunc = (void (*)(void))func;
    esp_timer_create_args_t timerArgs = {
        .callback = &nesTimerCallback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "nes_timer"
    };
    esp_timer_create(&timerArgs, &g_nesTimer);
    esp_timer_start_periodic(g_nesTimer, 1000000 / frequency);
    return 0;
}

static int cyd_logprint(const char *string) {
    Serial.print(string);
    return 0;
}

extern "C" int osd_init(void) {
    nofrendo_log_chain_logfunc(cyd_logprint);
    return 0;
}

extern "C" void osd_shutdown(void) {
    nes_timer_cleanup();
}

static char s_configFileName[] = "na";

extern "C" int osd_main(int argc, char *argv[]) {
    config.filename = s_configFileName;
    return main_loop(argv[0], system_autodetect);
}

// -------------------------------------------------------------
// Antaramuka Utama Pelancaran NoFrendo
// -------------------------------------------------------------
int runNofrendoGame(const char* romPath) {
    Serial.printf("[NES] Memulakan NoFrendo dengan ROM: %s\n", romPath ? romPath : "builtin");
    Serial.printf("[NES] Heap sebelum: free=%u, maxAlloc=%u\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());

    uint8_t* romBuf = NULL;
    size_t romSize = 0;
    bool needFree = false;

    bool isBuiltin = (!romPath || strcmp(romPath, "builtin") == 0 ||
                      strcmp(romPath, "/builtin") == 0 ||
                      strcasecmp(romPath, "Ice Climber") == 0 ||
                      strcasecmp(romPath, "Ice Climber.nes") == 0);

    if (isBuiltin) {
        Serial.println("[NES] Menggunakan ROM Terbina Dalam: Ice Climber (24.6 KB PROGMEM)");
        romBuf = (uint8_t*)g_iceClimberRom;
        romSize = ICE_CLIMBER_ROM_SIZE;
        needFree = false;
    } else {
        const char* sdPath = romPath;
        if (strncmp(sdPath, "/sd", 3) == 0) {
            sdPath = romPath + 3; // buang prefix "/sd" -> "/nes/filename.nes"
        }

        romBuf = readSdFileToBuffer(sdPath, &romSize);
        needFree = true;

        if (!romBuf || romSize == 0) {
            // Jika fail adalah Ice Climber dan gagal dibaca dari SD, fallback terus ke Flash!
            if (strstr(romPath, "Ice Climber") != NULL || strstr(romPath, "ice climber") != NULL ||
                strstr(romPath, "Ice_Climber") != NULL || strstr(romPath, "ice_climber") != NULL) {
                Serial.printf("[NES] Gagal baca '%s' dari SD, beralih ke ROM Flash Ice Climber!\n", sdPath);
                romBuf = (uint8_t*)g_iceClimberRom;
                romSize = ICE_CLIMBER_ROM_SIZE;
                needFree = false;
            } else {
                Serial.printf("[NES] GAGAL baca ROM dari kad SD! Laluan: '%s'\n", sdPath);
                return -1;
            }
        }
    }

    Serial.printf("[NES] ROM sedia: %u bait (needFree=%d). Heap: free=%u, maxAlloc=%u\n",
                  romSize, needFree ? 1 : 0, ESP.getFreeHeap(), ESP.getMaxAllocHeap());

    // Langkah 2: Tetapkan buffer pra-muat supaya NoFrendo guna fmemopen dengan zero-copy
    rom_set_preloaded_buffer(romBuf, romSize, needFree);

    if (g_pLcd) {
        g_pLcd->fillScreen(TFT_BLACK);
        drawNofrendoBezel();
    }

    // Langkah 3: Lancarkan NoFrendo
    char* argv[1];
    argv[0] = (char*)(romPath ? romPath : "Ice Climber.nes");
    int res = nofrendo_main(1, argv);

    // Langkah 4: Pembersihan
    nes_timer_cleanup();
    rom_clear_preloaded_buffer();
    Serial.printf("[NES] NoFrendo sesi selesai. Kod: %d, Heap: free=%u\n", res, ESP.getFreeHeap());
    return res;
}

void stopNofrendoGame() {
    main_quit();
    nes_timer_cleanup();
}
