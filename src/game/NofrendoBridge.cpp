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
        // 1. Sentuhan pada Butang Keluar [HUB] (x >= 265 && y < 42)
        if (tx >= 265 && ty < 42) {
            main_quit();
            return val;
        }

        // 2. Pad Arah Maya (Kawasan Kiri: tx < 70)
        if (tx < 70) {
            if (ty < 85)       val ^= (1 << 0); // ATAS
            else if (ty > 155) val ^= (1 << 1); // BAWAH
            else if (tx < 35)  val ^= (1 << 2); // KIRI
            else               val ^= (1 << 3); // KANAN
        }

        // 3. Butang Aksi Maya (Kawasan Kanan: tx >= 250)
        if (tx >= 250) {
            if (ty >= 45 && ty < 105)        val ^= (1 << 6); // A
            else if (ty >= 105 && ty < 165)  val ^= (1 << 7); // B
            else if (ty >= 165 && ty < 205)  val ^= (1 << 5); // START
            else if (ty >= 205)              val ^= (1 << 4); // SELECT
        }
    }

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
// Lukis Bezel Bingkai Maya (D-Pad & A/B)
// -------------------------------------------------------------
void drawNofrendoBezel() {
    if (!g_pLcd) return;
    g_pLcd->startWrite();
    
    // Bezel Kiri (x=0..31): Latar Belakang Gelap Kemas
    g_pLcd->fillRect(0, 0, 32, 240, g_pLcd->color565(15, 23, 42));
    g_pLcd->drawFastVLine(31, 0, 240, g_pLcd->color565(56, 189, 248));
    
    // D-Pad ATAS (y=30..62)
    g_pLcd->fillRoundRect(3, 30, 26, 32, 4, g_pLcd->color565(30, 41, 59));
    g_pLcd->drawRoundRect(3, 30, 26, 32, 4, TFT_SKYBLUE);
    g_pLcd->setTextColor(TFT_WHITE, g_pLcd->color565(30, 41, 59));
    g_pLcd->setTextSize(1);
    g_pLcd->drawCenterString("^", 16, 38);
    
    // D-Pad KIRI (y=75..107)
    g_pLcd->fillRoundRect(3, 75, 26, 32, 4, g_pLcd->color565(30, 41, 59));
    g_pLcd->drawRoundRect(3, 75, 26, 32, 4, TFT_SKYBLUE);
    g_pLcd->drawCenterString("<", 16, 83);

    // D-Pad KANAN (y=120..152)
    g_pLcd->fillRoundRect(3, 120, 26, 32, 4, g_pLcd->color565(30, 41, 59));
    g_pLcd->drawRoundRect(3, 120, 26, 32, 4, TFT_SKYBLUE);
    g_pLcd->drawCenterString(">", 16, 128);

    // D-Pad BAWAH (y=165..197)
    g_pLcd->fillRoundRect(3, 165, 26, 32, 4, g_pLcd->color565(30, 41, 59));
    g_pLcd->drawRoundRect(3, 165, 26, 32, 4, TFT_SKYBLUE);
    g_pLcd->drawCenterString("v", 16, 173);

    // Bezel Kanan (x=288..319): Butang Aksi & Navigasi
    g_pLcd->fillRect(288, 0, 32, 240, g_pLcd->color565(15, 23, 42));
    g_pLcd->drawFastVLine(288, 0, 240, g_pLcd->color565(56, 189, 248));

    // [HUB] Butang Keluar (y=4..32)
    g_pLcd->fillRoundRect(290, 4, 28, 28, 4, g_pLcd->color565(225, 29, 72));
    g_pLcd->setTextColor(TFT_WHITE, g_pLcd->color565(225, 29, 72));
    g_pLcd->drawCenterString("HUB", 304, 12);

    // [A] Butang A (y=48..82)
    g_pLcd->fillRoundRect(290, 48, 28, 34, 4, g_pLcd->color565(37, 99, 235));
    g_pLcd->drawRoundRect(290, 48, 28, 34, 4, TFT_WHITE);
    g_pLcd->setTextColor(TFT_WHITE, g_pLcd->color565(37, 99, 235));
    g_pLcd->drawCenterString("A", 304, 58);

    // [B] Butang B (y=98..132)
    g_pLcd->fillRoundRect(290, 98, 28, 34, 4, g_pLcd->color565(202, 138, 4));
    g_pLcd->drawRoundRect(290, 98, 28, 34, 4, TFT_WHITE);
    g_pLcd->setTextColor(TFT_WHITE, g_pLcd->color565(202, 138, 4));
    g_pLcd->drawCenterString("B", 304, 108);

    // [STA] Start (y=148..176)
    g_pLcd->fillRoundRect(290, 148, 28, 28, 4, g_pLcd->color565(16, 185, 129));
    g_pLcd->setTextColor(TFT_WHITE, g_pLcd->color565(16, 185, 129));
    g_pLcd->drawCenterString("STA", 304, 156);

    // [SEL] Select (y=190..218)
    g_pLcd->fillRoundRect(290, 190, 28, 28, 4, g_pLcd->color565(71, 85, 105));
    g_pLcd->setTextColor(TFT_WHITE, g_pLcd->color565(71, 85, 105));
    g_pLcd->drawCenterString("SEL", 304, 198);

    g_pLcd->endWrite();
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
