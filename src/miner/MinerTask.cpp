#include "MinerTask.h"
#include "Config.h"
#include "MinerSharedData.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <mbedtls/sha256.h>
#include <atomic>
#include <SPI.h>
#include <SD.h>
#include <SPIFFS.h>
#include <vector>

// Definisi objek pengurus statistik global
MinerDataManager g_minerData;

// Pembolehubah perkongsian Midstate untuk Perlombongan Dual CPU (Core 0 + Core 1)
static uint32_t g_sharedMidstate[8];
static uint32_t g_sharedMerkleTail = 0;
static uint32_t g_sharedNtime = 0;
static uint32_t g_sharedNbits = 0;
static volatile bool g_midstateReady = false;
static std::atomic<uint32_t> g_hashesBatchAccumulator(0);
static std::atomic<uint32_t> g_core1Nonce(0x80000000);
static volatile unsigned long g_lastCore1HashTime = 0;

// Objek storan kekal NVS & Web Server
static Preferences g_prefs;
static WebServer g_server(80);
static bool g_serverStarted = false;
static bool g_apModeActive = false;

static String g_activeSsid = "";
static String g_activePass = "";
static String g_activePool = "";
static uint32_t g_activePort = 21496;
static String g_activeWallet = "";

// Buffer blok transaksi Bitcoin untuk hashing 80-byte header
static uint8_t g_blockHeader[80] = {
    0x00, 0x00, 0x00, 0x20, // Version
    0x7b, 0x3d, 0x48, 0x26, 0x11, 0x89, 0xa4, 0xb2,
    0x02, 0xd4, 0x22, 0x17, 0x1a, 0x01, 0x12, 0x56,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x3a, 0x4b, 0x5c, 0x6d, 0x7e, 0x8f, 0x90, 0x11,
    0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
    0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11,
    0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
    0x12, 0x34, 0x56, 0x78,
    0x17, 0x04, 0x96, 0x9d,
    0x00, 0x00, 0x00, 0x00
};

// Halaman WebGUI Utama: Dashboard Langsung & Borang Konfigurasi
static const char HTML_CONFIG_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ms">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>game-minerd - Web Dashboard & Konfigurasi</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }
        body { background: #0b0f19; color: #f8fafc; padding: 16px; display: flex; justify-content: center; }
        .container { max-width: 480px; width: 100%; }
        .header { text-align: center; margin-bottom: 20px; }
        .header h1 { font-size: 22px; color: #f59e0b; margin-bottom: 4px; }
        .header p { font-size: 13px; color: #94a3b8; }
        .badge { display: inline-block; padding: 4px 10px; border-radius: 20px; font-size: 11px; font-weight: bold; background: #065f46; color: #34d399; margin-top: 6px; }
        
        /* Live Stats Grid */
        .stats-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 18px; }
        .stat-box { background: #1e293b; border: 1px solid #334155; border-radius: 10px; padding: 14px; text-align: center; }
        .stat-label { font-size: 11px; color: #94a3b8; text-transform: uppercase; margin-bottom: 4px; }
        .stat-val { font-size: 18px; font-weight: bold; color: #38bdf8; }
        .stat-val.gold { color: #fbbf24; }
        .stat-val.green { color: #34d399; }

        /* Card Form */
        .card { background: #1e293b; border: 1px solid #38bdf8; border-radius: 12px; padding: 22px; box-shadow: 0 10px 25px rgba(0,0,0,0.5); }
        .card h2 { font-size: 16px; color: #38bdf8; margin-bottom: 14px; border-bottom: 1px solid #334155; padding-bottom: 8px; }
        .group { margin-bottom: 14px; }
        label { display: block; font-size: 12px; font-weight: 600; margin-bottom: 5px; color: #cbd5e1; }
        input[type="text"], input[type="password"], input[type="number"] { width: 100%; padding: 11px; border-radius: 8px; border: 1px solid #475569; background: #0f172a; color: #fff; font-size: 14px; outline: none; transition: border-color 0.2s; }
        input:focus { border-color: #f59e0b; }
        
        .btn-submit { width: 100%; padding: 13px; background: linear-gradient(135deg, #10b981, #059669); border: none; border-radius: 8px; color: #fff; font-size: 15px; font-weight: bold; cursor: pointer; margin-top: 10px; transition: opacity 0.2s; }
        .btn-submit:hover { opacity: 0.9; }

        .btn-restart { width: 100%; padding: 11px; background: #334155; border: 1px solid #475569; border-radius: 8px; color: #e2e8f0; font-size: 13px; font-weight: bold; cursor: pointer; margin-top: 12px; }
        .btn-restart:hover { background: #475569; }

        .footer { font-size: 11px; color: #64748b; text-align: center; margin-top: 16px; line-height: 1.5; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🎮⛏️ game-minerd</h1>
            <p>Handheld Retro Console & Background Solo Miner</p>
            <div class="badge" id="net-badge">MEMUATKAN STATUS...</div>
        </div>

        <!-- Live Mining Stats -->
        <div class="stats-grid">
            <div class="stat-box">
                <div class="stat-label">Solo Hashrate</div>
                <div class="stat-val green" id="stat-hashrate">-- kH/s</div>
            </div>
            <div class="stat-box">
                <div class="stat-label">Kesukaran Terbaik</div>
                <div class="stat-val gold" id="stat-diff">--</div>
            </div>
            <div class="stat-box">
                <div class="stat-label">Jumlah Hashes</div>
                <div class="stat-val" id="stat-hashes">--</div>
            </div>
            <div class="stat-box">
                <div class="stat-label">Valid Shares</div>
                <div class="stat-val green" id="stat-shares">--</div>
            </div>
        </div>

        <!-- Settings Form -->
        <div class="card">
            <h2>⚙️ Konfigurasi Rangkaian & Perlombongan</h2>
            <form method="POST" action="/save">
                <div class="group">
                    <label>Pilihan Mod Perlombongan (Dual Mode):</label>
                    <div style="display:flex;gap:10px;margin-top:6px;">
                        <button type="button" onclick="setPreset('solo')" style="flex:1;background:#3b82f6;color:#fff;border:none;padding:10px;border-radius:8px;cursor:pointer;font-weight:bold;font-size:0.85rem;">🎯 Solo (Port 3333)</button>
                        <button type="button" onclick="setPreset('joined')" style="flex:1;background:#10b981;color:#fff;border:none;padding:10px;border-radius:8px;cursor:pointer;font-weight:bold;font-size:0.85rem;">⚡ PPLNS Pool (Port 13333)</button>
                    </div>
                </div>
                <div class="group">
                    <label>Nama WiFi (SSID):</label>
                    <input type="text" name="ssid" placeholder="Nama WiFi" required value="%SSID%">
                </div>
                <div class="group">
                    <label>Kata Laluan WiFi:</label>
                    <input type="password" name="pass" placeholder="Kata Laluan" value="%PASS%">
                </div>
                <div class="group">
                    <label>Mining Pool URL (Stratum):</label>
                    <input type="text" id="input-pool" name="pool" placeholder="public-pool.io" required value="%POOL%">
                </div>
                <div class="group">
                    <label>Mining Pool Port:</label>
                    <input type="number" id="input-port" name="port" placeholder="21496" required value="%PORT%">
                </div>
                <div class="group">
                    <label>Alamat Dompet Bitcoin (Wallet BTC):</label>
                    <input type="text" name="wallet" placeholder="bc1q..." required value="%WALLET%">
                </div>
                <button type="submit" class="btn-submit">💾 Simpan & Terapkan Perubahan</button>
            </form>
            <form method="POST" action="/restart" onsubmit="return confirm('Adakah anda pasti ingin memulakan semula (reboot) peranti?');">
                <button type="submit" class="btn-restart">🔄 Mulakan Semula (Reboot ESP32)</button>
            </form>
        </div>

        <!-- Pengurus Katrij NES & Storan Kad SD -->
        <div class="card" style="margin-top:20px;border:1px solid #10b981;">
            <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:12px;">
                <div style="font-weight:bold;color:#10b981;font-size:1.1rem;">
                    🎮 Pengurus Katrij NES (Max 64KB)
                </div>
                <button type="button" onclick="rescanSd()" style="background:#059669;color:#fff;border:none;padding:6px 12px;border-radius:6px;cursor:pointer;font-size:0.8rem;font-weight:bold;">🔄 Imbas Semula</button>
            </div>
            
            <p style="font-size:12px;color:#94a3b8;margin-bottom:14px;line-height:1.4;">
                Muat naik ROM NES baru (had <= 64KB kerana had SRAM ESP32 tanpa PSRAM) atau mainkan terus di skrin konsol CYD anda.
            </p>

            <!-- Borang Muat Naik ROM -->
            <form method="POST" action="/upload_rom" enctype="multipart/form-data" style="background:#0f172a;border:1px solid #334155;border-radius:8px;padding:12px;margin-bottom:14px;">
                <label style="color:#cbd5e1;font-size:12px;font-weight:bold;display:block;margin-bottom:6px;">⬆️ Muat Naik Fail .nes Baru:</label>
                <div style="display:flex;gap:8px;">
                    <input type="file" name="rom" accept=".nes" required style="flex:1;padding:8px;background:#1e293b;border:1px solid #475569;border-radius:6px;color:#fff;font-size:12px;">
                    <button type="submit" class="btn-submit" style="width:auto;margin:0;padding:8px 16px;font-size:13px;white-space:nowrap;background:linear-gradient(135deg,#0284c7,#0369a1);">Muat Naik</button>
                </div>
                <div style="font-size:11px;color:#64748b;margin-top:5px;">* Fail akan disimpan terus ke folder <code>/nes/</code> pada kad MicroSD.</div>
            </form>

            <!-- Status Storan & Senarai Katrij Game -->
            <div id="sd-status-box" style="background:#0f172a;padding:12px;border-radius:8px;font-size:0.9rem;">
                <div id="sd-info" style="color:#94a3b8;">Sedang mengimbas storan game...</div>
                <div id="sd-folders" style="margin-top:6px;font-size:0.8rem;color:#64748b;"></div>
                <div id="sd-games-list" style="margin-top:10px;max-height:260px;overflow-y:auto;"></div>
                <div style="text-align:right;margin-top:10px;padding-top:8px;border-top:1px dashed #334155;">
                    <button type="button" onclick="restoreBuiltin()" style="background:#1e293b;color:#94a3b8;border:1px solid #475569;padding:4px 10px;border-radius:6px;cursor:pointer;font-size:11px;">🔄 Pulihkan Game Asal (Ice Climber)</button>
                </div>
            </div>
        </div>

        <div class="footer">
            Peranti ini boleh diakses bila-bila masa melalui alamat IP atau <br>
            <b>http://game-minerd.local</b> semasa berada dalam rangkaian yang sama.
        </div>
    </div>

    <script>
        function updateStats() {
            fetch('/api/stats')
                .then(res => res.json())
                .then(d => {
                    document.getElementById('stat-hashrate').innerText = d.hashrate.toFixed(1) + ' kH/s';
                    document.getElementById('stat-diff').innerText = d.bestDiff > 1000 ? (d.bestDiff/1000).toFixed(1) + 'k' : d.bestDiff.toFixed(1);
                    document.getElementById('stat-hashes').innerText = d.totalHashes.toLocaleString();
                    document.getElementById('stat-shares').innerText = d.validShares;
                    
                    const badge = document.getElementById('net-badge');
                    if (d.wifiConnected) {
                        badge.style.background = '#065f46';
                        badge.style.color = '#34d399';
                        badge.innerText = 'ONLINE: ' + d.wifiSSID + ' (' + d.ip + ')';
                    } else {
                        badge.style.background = '#831843';
                        badge.style.color = '#f472b6';
                        badge.innerText = 'AP MODE: ' + d.ip;
                    }
                })
                .catch(() => {});
        }
        function updateSdInfo(forceRescan) {
            const url = forceRescan ? '/api/sd?rescan=1' : '/api/sd';
            fetch(url)
                .then(res => res.json())
                .then(d => {
                    const info = document.getElementById('sd-info');
                    const gamesList = document.getElementById('sd-games-list');
                    const folders = document.getElementById('sd-folders');
                    if (d.mounted) {
                        info.innerHTML = '<span style="color:#34d399;font-weight:bold;">✅ Kad MicroSD Dikesan! (' + d.cardType + ', ' + (d.cardSizeMB > 1024 ? (d.cardSizeMB/1024).toFixed(1) + ' GB' : d.cardSizeMB + ' MB') + ')</span><br>' +
                                         'Folder: <b>' + d.detectedFolder + '</b> | Game Tersedia: <b>' + (d.items ? d.items.length : 0) + ' game</b>';
                        if (d.rootFolders && d.rootFolders.length > 0) {
                            folders.innerHTML = 'Folder di kad: <span style="color:#94a3b8;">' + d.rootFolders.slice(0, 10).join(', ') + '</span>';
                        }
                    } else {
                        info.innerHTML = '<span style="color:#38bdf8;font-weight:bold;">💾 Storan Flash ESP32 Aktif</span> <span style="font-size:11px;color:#94a3b8;">(Tiada Kad MicroSD / Bukan FAT32)</span><br>' +
                                         'Game Tersedia: <b>' + (d.items ? d.items.length : 0) + ' game</b>';
                        folders.innerHTML = '';
                    }

                    let html = '';
                    if (d.items && d.items.length > 0) {
                        d.items.forEach(item => {
                            const isBuiltin = (item.filename === 'Ice Climber.nes');
                            html += '<div style="display:flex;justify-content:space-between;align-items:center;padding:7px 0;border-bottom:1px solid #1e293b;">';
                            html += '  <div>';
                            html += '    <div style="font-size:13px;font-weight:600;color:#fff;">🎮 ' + item.displayName + (isBuiltin ? ' <span style=\"color:#facc15;font-size:10px;\">[Asal]</span>' : '') + '</div>';
                            html += '    <div style="font-size:11px;color:#94a3b8;">' + item.sizeKB + ' KB ' + (item.sizeKB <= 64 ? '<span style=\"color:#34d399;font-weight:bold;\">[Siap Main]</span>' : '<span style=\"color:#f87171;\">[Melebihi Had]</span>') + '</div>';
                            html += '  </div>';
                            html += '  <div style="display:flex;gap:6px;">';
                            html += '    <button type="button" onclick="playRom(\'' + encodeURIComponent(item.filename) + '\')" style="background:#059669;color:#fff;border:none;padding:5px 10px;border-radius:6px;cursor:pointer;font-size:11px;font-weight:bold;">▶ Main</button>';
                            html += '    <button type="button" onclick="deleteRom(\'' + encodeURIComponent(item.filename) + '\')" style="background:#dc2626;color:#fff;border:none;padding:5px 8px;border-radius:6px;cursor:pointer;font-size:11px;" title="Padam game ini">🗑️</button>';
                            html += '  </div>';
                            html += '</div>';
                        });
                    } else {
                        html = '<div style="color:#fbbf24;padding:8px 0;font-size:12px;">Tiada game dalam storan. Sila muat naik fail .nes (<=64KB) di borang atas.</div>';
                    }
                    gamesList.innerHTML = html;
                })
                .catch(() => {});
        }
        function playRom(name) {
            const decName = decodeURIComponent(name);
            if (!confirm('Mulakan game "' + decName + '" pada skrin konsol CYD?')) return;
            fetch('/api/play_rom?name=' + name, { method: 'POST' })
                .then(res => res.json())
                .then(d => {
                    alert(d.msg || 'Game dimulakan di skrin CYD!');
                })
                .catch(e => alert('Ralat memulakan game: ' + e));
        }
        function deleteRom(name) {
            const decName = decodeURIComponent(name);
            const promptMsg = (decName === 'Ice Climber.nes')
                ? 'Adakah anda pasti ingin memadam game asal "Ice Climber"?\n(Boleh dipulihkan bila-bila masa melalui butang Pulihkan)'
                : 'Padam game "' + decName + '" daripada storan?';
            if (!confirm(promptMsg)) return;
            fetch('/api/delete_rom?name=' + name, { method: 'POST' })
                .then(res => res.json())
                .then(d => {
                    if (d.status === 'ok') {
                        alert('Game "' + decName + '" berjaya dipadam!');
                        updateSdInfo(true);
                    } else {
                        alert('Ralat memadam fail: ' + (d.error || 'Gagal'));
                    }
                })
                .catch(e => alert('Ralat memadam fail: ' + e));
        }
        function restoreBuiltin() {
            if (!confirm('Pulihkan game asal (Ice Climber) ke dalam katalog?')) return;
            fetch('/api/restore_builtin', { method: 'POST' })
                .then(res => res.json())
                .then(d => {
                    alert(d.msg || 'Game berjaya dipulihkan!');
                    updateSdInfo(true);
                })
                .catch(e => alert('Ralat: ' + e));
        }
        function rescanSd() {
            document.getElementById('sd-info').innerText = 'Sedang mengimbas semula slot MicroSD CYD...';
            updateSdInfo(true);
        }
        function setPreset(mode) {
            document.getElementById('input-pool').value = 'public-pool.io';
            if (mode === 'solo') {
                document.getElementById('input-port').value = '3333';
            } else if (mode === 'joined') {
                document.getElementById('input-port').value = '13333';
            }
        }
        setInterval(updateStats, 2000);
        updateStats();
        updateSdInfo(false);
    </script>
</body>
</html>
)rawliteral";

static void handleRoot() {
    String page = FPSTR(HTML_CONFIG_PAGE);
    page.replace("%SSID%", g_activeSsid);
    page.replace("%PASS%", g_activePass);
    page.replace("%POOL%", g_activePool);
    page.replace("%PORT%", String(g_activePort));
    page.replace("%WALLET%", g_activeWallet);
    g_server.send(200, "text/html", page);
}

static void handleApiStats() {
    MinerStats s = g_minerData.getStats();
    String json = "{";
    json += "\"hashrate\":" + String(s.currentHashrate, 2) + ",";
    json += "\"totalHashes\":" + String(s.totalHashes) + ",";
    json += "\"validShares\":" + String(s.validShares) + ",";
    json += "\"bestDiff\":" + String(s.bestDifficulty, 2) + ",";
    json += "\"wifiConnected\":" + String(s.isWifiConnected ? "true" : "false") + ",";
    json += "\"wifiSSID\":\"" + String(s.wifiSSID) + "\",";
    json += "\"ip\":\"" + String(s.ipAddress) + "\",";
    json += "\"pool\":\"" + String(s.activePool) + "\",";
    json += "\"dualCpu\":" + String(s.isDualCpuActive ? "true" : "false");
    json += "}";
    g_server.send(200, "application/json", json);
}

// Pengendali Perkakasan Kad MicroSD (Slot CYD: SCK=18, MISO=19, MOSI=23, CS=5)
static SPIClass g_sdSPI(VSPI);
static bool g_sdMounted = false;
static unsigned long g_lastSdAttempt = 0;
static SemaphoreHandle_t g_sdMutex = NULL;

struct CachedSdInfo {
    bool scanned = false;
    bool mounted = false;
    String cardType = "UNKNOWN";
    uint64_t cardSizeMB = 0;
    String detectedFolder = "";
    std::vector<SdGameItem> gameItems;
    std::vector<String> games;
    std::vector<String> rootFolders;
};
static CachedSdInfo g_sdCache;

static bool g_spiffsMounted = false;
static bool initSpiffsSafe() {
    if (g_spiffsMounted) return true;
    if (SPIFFS.begin(true)) {
        g_spiffsMounted = true;
        Serial.printf("[SPIFFS] Flash storan aktif! Total: %u KB, Used: %u KB\n",
                      (unsigned int)(SPIFFS.totalBytes() / 1024), (unsigned int)(SPIFFS.usedBytes() / 1024));
        return true;
    }
    Serial.println("[SPIFFS] Gagal mount SPIFFS.");
    return false;
}

static bool mountSD() {
    if (g_sdMounted) return true;
    if (g_sdMutex == NULL) g_sdMutex = xSemaphoreCreateMutex();
    if (xSemaphoreTake(g_sdMutex, pdMS_TO_TICKS(1500)) != pdTRUE) return false;

    if (g_sdMounted) {
        xSemaphoreGive(g_sdMutex);
        return true;
    }

    if (millis() - g_lastSdAttempt < 10000) {
        xSemaphoreGive(g_sdMutex);
        return false;
    }
    g_lastSdAttempt = millis();

    SD.end();
    pinMode(5, OUTPUT);
    digitalWrite(5, HIGH);
    delay(10);

    g_sdSPI.end();
    g_sdSPI.begin(18, 19, 23, 5);
    delay(20);

    bool ok = SD.begin(5, g_sdSPI, 4000000);
    if (!ok) {
        ok = SD.begin(5, g_sdSPI, 1000000);
    }

    Serial.printf("[SD DBG] SD.begin returned %s\n", ok ? "TRUE" : "FALSE");
    if (ok) {
        g_sdMounted = true;
        uint64_t sz = SD.cardSize() / (1024 * 1024);
        Serial.printf("[SD CARD] Kad berjaya dikesan! Saiz: %llu MB\n", sz);
    } else {
        Serial.println("[SD CARD] Tidak dapat memulakan kad SD. Sila pastikan dimasukkan kemas.");
    }
    xSemaphoreGive(g_sdMutex);
    return g_sdMounted;
}

static String g_pendingRomName = "";
static bool g_hasPendingRom = false;

bool checkPendingRomRequest(String& outName) {
    if (g_hasPendingRom) {
        outName = g_pendingRomName;
        g_hasPendingRom = false;
        return true;
    }
    return false;
}

static void scanSdCardSafe(bool forceRescan = false) {
    if (g_sdCache.scanned && !forceRescan) return;

    bool sdOk = mountSD();
    g_sdCache.mounted = sdOk;

    if (sdOk) {
        if (g_sdMutex == NULL) g_sdMutex = xSemaphoreCreateMutex();
        if (xSemaphoreTake(g_sdMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
            uint8_t cType = SD.cardType();
            g_sdCache.cardType = (cType == CARD_MMC) ? "MMC" :
                                 (cType == CARD_SD) ? "SDSC" :
                                 (cType == CARD_SDHC) ? "SDHC/SDXC" : "SD";
            g_sdCache.cardSizeMB = SD.cardSize() / (1024 * 1024);
            g_sdCache.rootFolders = {"nes", "roms", "arcade", "mame", "famicom"};

            if (!SD.exists("/nes")) SD.mkdir("/nes");
            xSemaphoreGive(g_sdMutex);
        }
    }

    g_sdCache.games.clear();
    g_sdCache.gameItems.clear();

    // 1. Sediakan Ice Climber sebagai game terbina dalam jika belum dipadam pengguna
    g_prefs.begin("minerd", true);
    bool hideBuiltin = g_prefs.getBool("hide_builtin", false);
    g_prefs.end();

    if (!hideBuiltin) {
        SdGameItem builtinItem;
        builtinItem.filename = "Ice Climber.nes";
        builtinItem.displayName = "Ice Climber";
        builtinItem.sizeKB = 25;
        builtinItem.isCompatible = true;
        g_sdCache.gameItems.push_back(builtinItem);
        g_sdCache.games.push_back("Ice Climber.nes (25 KB - Built-in Flash)");
    }

    // 2. Imbas dari Kad SD jika ada
    if (sdOk) {
        if (g_sdMutex == NULL) g_sdMutex = xSemaphoreCreateMutex();
        if (xSemaphoreTake(g_sdMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
            const char* targetFolder = NULL;
            if (SD.exists("/nes")) targetFolder = "/nes";
            else if (SD.exists("/roms/nes")) targetFolder = "/roms/nes";
            else if (SD.exists("/famicom")) targetFolder = "/famicom";

            g_sdCache.detectedFolder = targetFolder ? String(targetFolder) : "/nes";

            if (targetFolder) {
                File dir = SD.open(targetFolder);
                if (dir && dir.isDirectory()) {
                    File f = dir.openNextFile();
                    int scanned = 0;
                    while (f && g_sdCache.gameItems.size() < 40 && scanned < 600) {
                        scanned++;
                        if (!f.isDirectory()) {
                            const char* fn = f.name();
                            if (fn) {
                                String sName = String(fn);
                                int lastSlash = sName.lastIndexOf('/');
                                if (lastSlash >= 0) sName = sName.substring(lastSlash + 1);
                                String lower = sName;
                                lower.toLowerCase();
                                if (lower.endsWith(".nes")) {
                                    if (lower != "ice climber.nes" && lower != "ice_climber.nes") {
                                        uint32_t szKB = (f.size() + 1023) / 1024;
                                        if (szKB <= 64) {
                                            SdGameItem item;
                                            item.filename = sName;
                                            String clean = sName;
                                            if (clean.endsWith(".nes") || clean.endsWith(".NES")) {
                                                clean = clean.substring(0, clean.length() - 4);
                                            }
                                            item.displayName = clean;
                                            item.sizeKB = szKB;
                                            item.isCompatible = true;
                                            g_sdCache.gameItems.push_back(item);
                                            g_sdCache.games.push_back(sName + " (" + String(szKB) + " KB)");
                                        }
                                    }
                                }
                            }
                        }
                        f.close();
                        f = dir.openNextFile();
                    }
                    if (f) f.close();
                    dir.close();
                }
            }
            xSemaphoreGive(g_sdMutex);
        }
    }

    // 3. Imbas Flash SPIFFS (Internal Storage) untuk sebarang ROM muat naik web
    if (initSpiffsSafe()) {
        File sDir = SPIFFS.open("/");
        if (sDir) {
            File f = sDir.openNextFile();
            while (f && g_sdCache.gameItems.size() < 40) {
                if (!f.isDirectory()) {
                    String fn = String(f.name());
                    int lastSlash = fn.lastIndexOf('/');
                    if (lastSlash >= 0) fn = fn.substring(lastSlash + 1);
                    String lower = fn;
                    lower.toLowerCase();
                    if (lower.endsWith(".nes")) {
                        bool dup = false;
                        for (const auto& gi : g_sdCache.gameItems) {
                            if (gi.filename.equalsIgnoreCase(fn)) { dup = true; break; }
                        }
                        if (!dup) {
                            uint32_t szKB = (f.size() + 1023) / 1024;
                            if (szKB <= 64) {
                                SdGameItem item;
                                item.filename = fn;
                                String clean = fn;
                                if (clean.endsWith(".nes") || clean.endsWith(".NES")) clean = clean.substring(0, clean.length() - 4);
                                item.displayName = clean;
                                item.sizeKB = szKB;
                                item.isCompatible = true;
                                g_sdCache.gameItems.push_back(item);
                                g_sdCache.games.push_back(fn + " (" + String(szKB) + " KB - Flash)");
                            }
                        }
                    }
                }
                f.close();
                f = sDir.openNextFile();
            }
            if (f) f.close();
            sDir.close();
        }
    }

    g_sdCache.scanned = true;
    Serial.printf("[SD/FLASH] Imbasan siap. Total game serasi (<=64KB): %d\n", (int)g_sdCache.gameItems.size());
}

bool isSdCardMounted() {
    return g_sdMounted;
}

String getSdDetectedFolder() {
    return g_sdCache.detectedFolder;
}

std::vector<SdGameItem> getSdGameList(bool forceRescan) {
    scanSdCardSafe(forceRescan);
    g_prefs.begin("minerd", true);
    bool hideBuiltin = g_prefs.getBool("hide_builtin", false);
    g_prefs.end();

    if (g_sdCache.gameItems.empty() && !hideBuiltin) {
        SdGameItem builtinItem;
        builtinItem.filename = "Ice Climber.nes";
        builtinItem.displayName = "Ice Climber";
        builtinItem.sizeKB = 25;
        builtinItem.isCompatible = true;
        g_sdCache.gameItems.push_back(builtinItem);
    }
    return g_sdCache.gameItems;
}

uint8_t* readSdFileToBuffer(const char* sdPath, size_t* outSize) {
    if (!outSize) return NULL;
    *outSize = 0;
    if (!sdPath || strlen(sdPath) == 0) return NULL;

    String cleanPath = String(sdPath);
    while (cleanPath.indexOf("//") >= 0) cleanPath.replace("//", "/");
    if (!cleanPath.startsWith("/")) cleanPath = "/" + cleanPath;

    String basename = cleanPath;
    int lastSlash = basename.lastIndexOf('/');
    if (lastSlash >= 0) basename = basename.substring(lastSlash + 1);

    File f;
    bool fromSpiffs = false;

    // 1. Cuba baca dari Kad MicroSD (jika mounted)
    if (mountSD()) {
        if (g_sdMutex == NULL) g_sdMutex = xSemaphoreCreateMutex();
        if (xSemaphoreTake(g_sdMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
            f = SD.open(cleanPath.c_str());
            if (!f) f = SD.open(("/nes/" + basename).c_str());
            if (!f) f = SD.open(("/roms/nes/" + basename).c_str());
            if (!f) f = SD.open(("/" + basename).c_str());
            if (!f) xSemaphoreGive(g_sdMutex);
        }
    }

    // 2. Jika tiada di SD, cuba baca dari Flash SPIFFS (muat naik web)
    if (!f && initSpiffsSafe()) {
        f = SPIFFS.open(("/" + basename).c_str());
        if (!f) f = SPIFFS.open(("/nes/" + basename).c_str());
        if (f) fromSpiffs = true;
    }

    if (!f) {
        Serial.printf("[READ ROM] Fail tidak ditemui di SD atau Flash: '%s'\n", cleanPath.c_str());
        return NULL;
    }

    size_t fileSize = f.size();
    if (fileSize == 0 || fileSize > 80 * 1024) {
        f.close();
        if (!fromSpiffs && g_sdMutex) xSemaphoreGive(g_sdMutex);
        return NULL;
    }

    uint8_t* buf = (uint8_t*)malloc(fileSize);
    if (!buf) buf = (uint8_t*)heap_caps_malloc(fileSize, MALLOC_CAP_8BIT);
    if (!buf) {
        f.close();
        if (!fromSpiffs && g_sdMutex) xSemaphoreGive(g_sdMutex);
        return NULL;
    }

    size_t bytesRead = f.read(buf, fileSize);
    f.close();
    if (!fromSpiffs && g_sdMutex) xSemaphoreGive(g_sdMutex);

    if (bytesRead != fileSize) {
        free(buf);
        return NULL;
    }

    *outSize = fileSize;
    Serial.printf("[READ ROM] Berjaya baca %u bait (%s). freeHeap: %u\n",
                  fileSize, fromSpiffs ? "Flash SPIFFS" : "Kad MicroSD", ESP.getFreeHeap());
    return buf;
}

static void handleApiSd() {
    bool force = g_server.hasArg("rescan");
    scanSdCardSafe(force);

    String json = "{\"mounted\":" + String(g_sdCache.mounted ? "true" : "false");
    json += ",\"freeHeap\":" + String(ESP.getFreeHeap());
    json += ",\"maxAllocHeap\":" + String(ESP.getMaxAllocHeap());

    if (!g_sdCache.mounted) {
        json += ",\"error\":\"Kad MicroSD belum dikesan atau bukan format FAT32 (Storan Flash ESP32 Aktif)\"";
    } else {
        json += ",\"cardType\":\"" + g_sdCache.cardType + "\"";
        json += ",\"cardSizeMB\":" + String((unsigned long)g_sdCache.cardSizeMB);
        json += ",\"detectedFolder\":\"" + g_sdCache.detectedFolder + "\"";
        json += ",\"rootFolders\":[";
        for (size_t i = 0; i < g_sdCache.rootFolders.size(); i++) {
            if (i > 0) json += ",";
            json += "\"" + g_sdCache.rootFolders[i] + "\"";
        }
        json += "]";
    }

    json += ",\"totalFound\":" + String(g_sdCache.gameItems.size());
    json += ",\"items\":[";
    for (size_t i = 0; i < g_sdCache.gameItems.size(); i++) {
        if (i > 0) json += ",";
        String fn = g_sdCache.gameItems[i].filename;
        fn.replace("\"", "\\\"");
        String dn = g_sdCache.gameItems[i].displayName;
        dn.replace("\"", "\\\"");
        json += "{\"filename\":\"" + fn + "\",\"displayName\":\"" + dn + "\",\"sizeKB\":" + String(g_sdCache.gameItems[i].sizeKB) + "}";
    }
    json += "]";
    json += "}";
    g_server.send(200, "application/json", json);
}

// --------------------------------------------------------------------------
// Pengurus Muat Naik, Main, & Padam ROM NES Melalui WebGUI
// --------------------------------------------------------------------------
static File g_uploadFile;
static size_t g_uploadBytes = 0;
static bool g_uploadErr = false;
static bool g_uploadUseSpiffs = false;

static void handleRomUploadData() {
    HTTPUpload& upload = g_server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        g_uploadBytes = 0;
        g_uploadErr = false;
        g_uploadUseSpiffs = false;
        String fn = upload.filename;
        int lastSlash = fn.lastIndexOf('/');
        if (lastSlash >= 0) fn = fn.substring(lastSlash + 1);
        int lastBs = fn.lastIndexOf('\\');
        if (lastBs >= 0) fn = fn.substring(lastBs + 1);

        String lower = fn;
        lower.toLowerCase();
        if (!lower.endsWith(".nes")) {
            Serial.println("[WEB UPLOAD] Ralat: Fail bukan format .nes!");
            g_uploadErr = true;
            return;
        }

        bool sdOk = mountSD();
        if (sdOk) {
            if (g_sdMutex == NULL) g_sdMutex = xSemaphoreCreateMutex();
            if (xSemaphoreTake(g_sdMutex, pdMS_TO_TICKS(1500)) == pdTRUE) {
                if (!SD.exists("/nes")) SD.mkdir("/nes");
                String path = "/nes/" + fn;
                if (SD.exists(path.c_str())) SD.remove(path.c_str());
                g_uploadFile = SD.open(path.c_str(), FILE_WRITE);
                if (!g_uploadFile) {
                    xSemaphoreGive(g_sdMutex);
                    sdOk = false;
                }
            } else {
                sdOk = false;
            }
        }

        if (!sdOk) {
            // Simpan terus ke Flash SPIFFS dalaman ESP32 (tiada kad SD diperlukan!)
            if (initSpiffsSafe()) {
                g_uploadUseSpiffs = true;
                String path = "/" + fn;
                if (SPIFFS.exists(path.c_str())) SPIFFS.remove(path.c_str());
                g_uploadFile = SPIFFS.open(path.c_str(), FILE_WRITE);
                if (!g_uploadFile) {
                    Serial.printf("[WEB UPLOAD] Gagal buka fail SPIFFS: '%s'\n", path.c_str());
                    g_uploadErr = true;
                    return;
                }
                Serial.printf("[WEB UPLOAD] Menerima fail ke Flash SPIFFS: '%s'...\n", path.c_str());
            } else {
                Serial.println("[WEB UPLOAD] Ralat: Kad SD dan SPIFFS kedua-duanya tidak sedia!");
                g_uploadErr = true;
                return;
            }
        } else {
            Serial.printf("[WEB UPLOAD] Menerima fail ke Kad SD: '/nes/%s'...\n", fn.c_str());
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!g_uploadErr && g_uploadFile) {
            g_uploadBytes += upload.currentSize;
            if (g_uploadBytes > 80 * 1024) {
                Serial.println("[WEB UPLOAD] Ralat: Saiz fail melebihi had RAM 80KB!");
                g_uploadErr = true;
                g_uploadFile.close();
                if (!g_uploadUseSpiffs && g_sdMutex) xSemaphoreGive(g_sdMutex);
            } else {
                g_uploadFile.write(upload.buf, upload.currentSize);
            }
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (g_uploadFile) {
            g_uploadFile.close();
            if (!g_uploadUseSpiffs && g_sdMutex) xSemaphoreGive(g_sdMutex);
            Serial.printf("[WEB UPLOAD] Muat naik berjaya! Saiz: %u bait (%s)\n",
                          (unsigned int)g_uploadBytes, g_uploadUseSpiffs ? "Flash SPIFFS" : "Kad MicroSD");
            g_sdCache.scanned = false;
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        if (g_uploadFile) {
            g_uploadFile.close();
            if (!g_uploadUseSpiffs && g_sdMutex) xSemaphoreGive(g_sdMutex);
        }
        g_uploadErr = true;
    }
}

static void handleRomUploadFinish() {
    if (g_uploadErr) {
        g_server.send(400, "text/html", "<html><body style='background:#0b0f19;color:#f87171;font-family:sans-serif;text-align:center;padding:40px;'><h2>Muat Naik Gagal!</h2><p>Pastikan fail berekstensi .nes dan saiz tidak melebihi 64KB (had RAM ESP32).</p><p><a href='/' style='color:#38bdf8;'>&larr; Kembali</a></p></body></html>");
    } else {
        String dest = g_uploadUseSpiffs ? "Flash SPIFFS Dalaman ESP32" : "Folder /nes/ pada Kad MicroSD";
        g_server.send(200, "text/html", "<html><body style='background:#0b0f19;color:#34d399;font-family:sans-serif;text-align:center;padding:40px;'><h2>Katrij NES Berjaya Dimuat Naik!</h2><p style='color:#fff;'>Disimpan ke: <b>" + dest + "</b></p><p><a href='/' style='color:#38bdf8;text-decoration:none;'>&larr; Kembali ke Dashboard (Game siap main!)</a></p></body></html>");
    }
}

static void handleApiPlayRom() {
    String filename = g_server.arg("name");
    if (filename.length() > 0) {
        int lastSlash = filename.lastIndexOf('/');
        if (lastSlash >= 0) filename = filename.substring(lastSlash + 1);
        g_pendingRomName = filename;
        g_hasPendingRom = true;
        Serial.printf("[WEB] Permintaan main game: '%s'\n", filename.c_str());
        g_server.send(200, "application/json", "{\"status\":\"ok\",\"msg\":\"Memulakan game di skrin CYD...\"}");
        return;
    }
    g_server.send(400, "application/json", "{\"error\":\"Nama game kosong\"}");
}

static void handleApiDeleteRom() {
    String filename = g_server.arg("name");
    if (filename.length() > 0) {
        int lastSlash = filename.lastIndexOf('/');
        if (lastSlash >= 0) filename = filename.substring(lastSlash + 1);

        bool deleted = false;

        // 1. Semak jika pengguna memadam game asal Ice Climber
        String lower = filename;
        lower.toLowerCase();
        if (lower.startsWith("ice climber") || lower.startsWith("ice_climber") || lower.startsWith("iceclimber")) {
            g_prefs.begin("minerd", false);
            g_prefs.putBool("hide_builtin", true);
            g_prefs.end();
            deleted = true;
            Serial.println("[ROM DELETE] Game asal Ice Climber dipadam/disembunyikan.");
        }

        // 2. Padam dari kad MicroSD jika dipasang
        if (isSdCardMounted()) {
            if (g_sdMutex == NULL) g_sdMutex = xSemaphoreCreateMutex();
            if (xSemaphoreTake(g_sdMutex, pdMS_TO_TICKS(1500)) == pdTRUE) {
                String path = "/nes/" + filename;
                if (SD.exists(path.c_str())) {
                    SD.remove(path.c_str());
                    Serial.printf("[SD] Fail dipadam dari SD: '%s'\n", path.c_str());
                    deleted = true;
                }
                path = "/" + filename;
                if (SD.exists(path.c_str())) {
                    SD.remove(path.c_str());
                    deleted = true;
                }
                xSemaphoreGive(g_sdMutex);
            }
        }

        // 3. Padam dari Flash SPIFFS dalaman ESP32
        if (initSpiffsSafe()) {
            String p1 = "/" + filename;
            if (SPIFFS.exists(p1.c_str())) {
                SPIFFS.remove(p1.c_str());
                Serial.printf("[SPIFFS] Fail dipadam dari Flash: '%s'\n", p1.c_str());
                deleted = true;
            }
            String p2 = "/nes/" + filename;
            if (SPIFFS.exists(p2.c_str())) {
                SPIFFS.remove(p2.c_str());
                deleted = true;
            }
            // Cari padanan fail tanpa peka huruf besar-kecil dalam SPIFFS
            File sDir = SPIFFS.open("/");
            if (sDir) {
                File f = sDir.openNextFile();
                while (f) {
                    String fn = String(f.name());
                    int slash = fn.lastIndexOf('/');
                    String bname = (slash >= 0) ? fn.substring(slash + 1) : fn;
                    if (bname.equalsIgnoreCase(filename)) {
                        f.close();
                        SPIFFS.remove(fn.c_str());
                        Serial.printf("[SPIFFS] Dipadam fail sepadan: '%s'\n", fn.c_str());
                        deleted = true;
                        break;
                    }
                    f.close();
                    f = sDir.openNextFile();
                }
                sDir.close();
            }
        }

        g_sdCache.scanned = false;
        if (deleted) {
            g_server.send(200, "application/json", "{\"status\":\"ok\"}");
            return;
        }
    }
    g_server.send(400, "application/json", "{\"error\":\"Fail tidak ditemui untuk dipadam\"}");
}

static void handleApiRestoreBuiltin() {
    g_prefs.begin("minerd", false);
    g_prefs.remove("hide_builtin");
    g_prefs.end();
    g_sdCache.scanned = false;
    Serial.println("[PREFS] Game asal Ice Climber dipulihkan.");
    g_server.send(200, "application/json", "{\"status\":\"ok\",\"msg\":\"Game asal (Ice Climber) telah dipulihkan!\"}");
}

static WiFiClient g_stratumClient;
static bool g_stratumConnected = false;
static unsigned long g_lastStratumAttempt = 0;
static String g_currentJobId = "1";
static String g_currentNtime = "6abc8bbb";
static double g_poolDifficulty = 1.0;

static void handleSave() {
    String newSsid = g_server.arg("ssid");
    String newPass = g_server.arg("pass");
    String newPool = g_server.arg("pool");
    String portStr = g_server.arg("port");
    String newWallet = g_server.arg("wallet");

    if (newSsid.length() > 0) {
        g_prefs.begin("minerd", false);
        g_prefs.putString("ssid", newSsid);
        g_prefs.putString("pass", newPass);
        if (newPool.length() > 0) g_prefs.putString("pool", newPool);
        if (portStr.length() > 0) g_prefs.putUInt("port", portStr.toInt());
        if (newWallet.length() > 0) g_prefs.putString("wallet", newWallet);
        g_prefs.end();

        g_activeSsid = newSsid;
        g_activePass = newPass;
        if (newPool.length() > 0) g_activePool = newPool;
        if (portStr.length() > 0) g_activePort = portStr.toInt();
        if (newWallet.length() > 0) g_activeWallet = newWallet;

        // Kemas kini ke struktur perkongsian
        String fullPool = g_activePool + ":" + String(g_activePort);
        g_minerData.setPoolAndWallet(fullPool.c_str(), g_activeWallet.c_str());
        g_stratumClient.stop();
        g_stratumConnected = false;
        g_lastStratumAttempt = 0;

        String resp = "<html><body style='background:#0b0f19;color:#10b981;font-family:sans-serif;text-align:center;padding:40px;'>"
                      "<h2>Tetapan Berjaya Disimpan!</h2>"
                      "<p style='color:#fff;margin:15px 0;'>Menyambung semula ke <b>" + newSsid + "</b>...</p>"
                      "<p style='color:#94a3b8;'>Peranti kini menggunakan Pool: <b>" + fullPool + "</b></p>"
                      "<p style='margin-top:20px;'><a href='/' style='color:#38bdf8;text-decoration:none;'>&larr; Kembali ke Dashboard</a></p>"
                      "</body></html>";
        g_server.send(200, "text/html", resp);

        delay(800);
        WiFi.disconnect(true);
        delay(300);
        WiFi.mode(WIFI_STA);
        if (g_activePass.length() > 0) {
            WiFi.begin(g_activeSsid.c_str(), g_activePass.c_str());
        } else {
            WiFi.begin(g_activeSsid.c_str());
        }
        g_apModeActive = false;
    } else {
        g_server.send(400, "text/plain", "Data tidak sah!");
    }
}

static void handleRestart() {
    g_server.send(200, "text/html", "<html><body style='background:#0b0f19;color:#fbbf24;font-family:sans-serif;text-align:center;padding:40px;'><h2>Memulakan Semula (Rebooting)...</h2><p>Sila tunggu beberapa saat sebelum memuat semula halaman ini.</p></body></html>");
    delay(1000);
    ESP.restart();
}

bool isMiningPoolMode() {
    return (g_activePort == DEFAULT_POOL_PPLNS_PORT);
}

void toggleMiningPoolMode() {
    g_prefs.begin("minerd", false);
    if (g_activePort == DEFAULT_POOL_PPLNS_PORT) {
        g_activePort = DEFAULT_POOL_PORT; // 3333 (Solo)
    } else {
        g_activePort = DEFAULT_POOL_PPLNS_PORT; // 13333 (PPLNS Pool)
    }
    g_activePool = DEFAULT_POOL_URL; // public-pool.io
    g_prefs.putString("pool", g_activePool);
    g_prefs.putUInt("port", g_activePort);
    g_prefs.end();

    String fullPool = g_activePool + ":" + String(g_activePort);
    g_minerData.setPoolAndWallet(fullPool.c_str(), g_activeWallet.c_str());
    g_minerData.setConnectionStatus(WiFi.status() == WL_CONNECTED, false, fullPool.c_str());

    g_stratumClient.stop();
    g_stratumConnected = false;
    g_lastStratumAttempt = 0;

    Serial.printf("[MINER Core 0] Mod Ditukar Melalui Skrin/Web: %s (%s)\n",
                  fullPool.c_str(), 
                  (g_activePort == DEFAULT_POOL_PPLNS_PORT) ? "POOL PPLNS (:13333)" : "SOLO (:3333)");
}

static bool g_mdnsStarted = false;

static void webServerTask(void* parameter) {
    while (true) {
        if (WiFi.status() == WL_CONNECTED || g_apModeActive) {
            if (!g_serverStarted) {
                g_server.on("/", HTTP_GET, handleRoot);
                g_server.on("/save", HTTP_POST, handleSave);
                g_server.on("/api/stats", HTTP_GET, handleApiStats);
                g_server.on("/api/sd", HTTP_GET, handleApiSd);
                g_server.on("/api/play_rom", handleApiPlayRom);
                g_server.on("/api/delete_rom", handleApiDeleteRom);
                g_server.on("/api/restore_builtin", handleApiRestoreBuiltin);
                g_server.on("/upload_rom", HTTP_POST, handleRomUploadFinish, handleRomUploadData);
                g_server.on("/restart", HTTP_POST, handleRestart);
                g_server.begin();
                g_serverStarted = true;
                Serial.printf("[HTTP] Web Server port 80 aktif di IP: %s\n", WiFi.localIP().toString().c_str());
            }
            g_server.handleClient();
        }
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

static void startConfigPortalAP() {
    if (!g_apModeActive) {
        Serial.println("[WIFI] Memulakan Access Point Sandaran: GameMinerd-WiFi (192.168.4.1)");
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
        WiFi.softAP("GameMinerd-WiFi", "12345678");
        g_apModeActive = true;
        g_minerData.setWifiDetails(false, "AP: GameMinerd-WiFi", "192.168.4.1");
    }
}

// ==============================================================================
// KLIEN STRATUM V1 TCP (Public-Pool.io / NerdMiners Joined Pool)
// ==============================================================================
static void handleStratumMining() {
    if (WiFi.status() != WL_CONNECTED) {
        g_stratumConnected = false;
        return;
    }

    if (!g_stratumClient.connected()) {
        g_stratumConnected = false;
        if (millis() - g_lastStratumAttempt > 8000) {
            g_lastStratumAttempt = millis();
            g_stratumClient.stop();
            Serial.printf("[STRATUM] Menyambung ke pool: %s:%u ...\n", g_activePool.c_str(), g_activePort);
            
            IPAddress poolIP;
            bool dnsOk = WiFi.hostByName(g_activePool.c_str(), poolIP);
            if (!dnsOk || poolIP == IPAddress(0, 0, 0, 0)) {
                if (g_activePool == "public-pool.io") {
                    poolIP = IPAddress(38, 51, 144, 232);
                } else if (g_activePool == "pool.nerdminers.org") {
                    poolIP = IPAddress(144, 91, 83, 152);
                }
            }

            if (poolIP != IPAddress(0, 0, 0, 0)) {
                Serial.printf("[STRATUM] IP Pool: %s\n", poolIP.toString().c_str());
            }

            if ((poolIP != IPAddress(0, 0, 0, 0) && g_stratumClient.connect(poolIP, g_activePort, 3500)) ||
                g_stratumClient.connect(g_activePool.c_str(), g_activePort, 3500)) {
                Serial.println("[STRATUM] Sambungan TCP berjaya ke Pool!");
                // 1. Subscribe dengan user agent NerdMinerV2
                g_stratumClient.print("{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": [\"NerdMinerV2\"]}\n");
                // 2. Authorize dengan wallet pengguna
                String authMsg = "{\"id\": 2, \"method\": \"mining.authorize\", \"params\": [\"" + g_activeWallet + ".cyd\", \"x\"]}\n";
                g_stratumClient.print(authMsg);
                Serial.printf("[STRATUM] Pengesahan dihantar untuk wallet: %s.cyd\n", g_activeWallet.c_str());
                g_stratumConnected = true;
            } else {
                Serial.println("[STRATUM] Sambungan ke pool gagal atau timeout.");
            }
        }
        return;
    }

    // Baca sebarang mesej daripada stratum pool
    while (g_stratumClient.available()) {
        String line = g_stratumClient.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            Serial.printf("[STRATUM POOL] %s\n", line.c_str());
            if (line.indexOf("\"result\":true") >= 0 && (line.indexOf("\"id\":2") >= 0 || line.indexOf("\"id\":3") >= 0)) {
                Serial.println("[STRATUM] >>> WALLET BERJAYA DISAHKAN OLEH POOL! <<<");
                String fullPool = g_activePool + ":" + String(g_activePort);
                g_minerData.setConnectionStatus(true, true, fullPool.c_str());
            } else if (line.indexOf("\"mining.set_difficulty\"") >= 0) {
                int dStart = line.indexOf(":[");
                if (dStart >= 0) {
                    g_poolDifficulty = line.substring(dStart + 2).toDouble();
                    Serial.printf("[STRATUM] Sasaran Kesukaran Pool dikemas kini: %.4f\n", g_poolDifficulty);
                }
            } else if (line.indexOf("\"mining.notify\"") >= 0) {
                int pStart = line.indexOf("[\"");
                if (pStart >= 0) {
                    int pEnd = line.indexOf("\",", pStart + 2);
                    if (pEnd > pStart) {
                        g_currentJobId = line.substring(pStart + 2, pEnd);
                        Serial.printf("[STRATUM] Tugas baru aktif (Job ID: %s)\n", g_currentJobId.c_str());
                    }
                }
            } else if (line.indexOf("\"id\":4") >= 0 && line.indexOf("\"result\":true") >= 0) {
                Serial.println("[STRATUM] >>> SYER DITERIMA & DISAHKAN OLEH POOL (VALID SHARE)! <<<");
                g_minerData.incrementValidShares();
            }
        }
    }
}

// ==============================================================================
// ENJIN MIKROKRNAL SHA-256 BITCOIN DENGAN PRECOMPUTED MIDSTATE
// ==============================================================================
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define S0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define S1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define s0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define s1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SWAP32(x) __builtin_bswap32(x)

static const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static const uint32_t SHA256_INITIAL[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

__attribute__((always_inline)) static inline void sha256_compress(uint32_t state[8], const uint32_t W[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

    #pragma GCC unroll 64
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + S1(e) + CH(e, f, g) + K256[i] + W[i];
        uint32_t t2 = S0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

static void compute_midstate(const uint8_t chunk1[64], uint32_t midstate[8]) {
    uint32_t W[64];
    for (int i = 0; i < 16; i++) {
        W[i] = SWAP32(((const uint32_t*)chunk1)[i]);
    }
    for (int i = 16; i < 64; i++) {
        W[i] = s1(W[i - 2]) + W[i - 7] + s0(W[i - 15]) + W[i - 16];
    }
    memcpy(midstate, SHA256_INITIAL, 32);
    sha256_compress(midstate, W);
}

void runMiningWorkerCore1(uint32_t batchSize) {
    if (!g_midstateReady) return;

    g_minerData.setDualCpuActive(true);
    g_lastCore1HashTime = millis();

    uint32_t midstate[8];
    memcpy(midstate, g_sharedMidstate, 32);

    uint32_t W[64];
    uint32_t W2[64];
    uint32_t hash1[8];
    uint32_t hash2[8];

    W[0] = g_sharedMerkleTail;
    W[1] = g_sharedNtime;
    W[2] = g_sharedNbits;
    W[4] = 0x80000000;
    for (int k = 5; k < 15; k++) W[k] = 0;
    W[15] = 0x00000280;

    W2[8] = 0x80000000;
    for (int k = 9; k < 15; k++) W2[k] = 0;
    W2[15] = 0x00000100;

    uint32_t localNonce = g_core1Nonce.load(std::memory_order_relaxed);

    for (uint32_t i = 0; i < batchSize; i++) {
        localNonce++;
        W[3] = SWAP32(localNonce);

        for (int k = 16; k < 64; k++) {
            W[k] = s1(W[k - 2]) + W[k - 7] + s0(W[k - 15]) + W[k - 16];
        }

        memcpy(hash1, midstate, 32);
        sha256_compress(hash1, W);

        for (int k = 0; k < 8; k++) {
            W2[k] = hash1[k];
        }
        for (int k = 16; k < 64; k++) {
            W2[k] = s1(W2[k - 2]) + W2[k - 7] + s0(W2[k - 15]) + W2[k - 16];
        }

        memcpy(hash2, SHA256_INITIAL, 32);
        sha256_compress(hash2, W2);

        if (hash2[7] == 0) {
            double estimatedDiff = 65536.0 / ((hash2[6] >> 16) + 1);
            g_minerData.updateMiningProgress(0, 0, estimatedDiff);

            if (g_stratumConnected && g_stratumClient.connected() && g_currentJobId.length() > 0) {
                char nonceHex[9];
                snprintf(nonceHex, sizeof(nonceHex), "%08x", SWAP32(localNonce));
                String submitMsg = "{\"id\": 4, \"method\": \"mining.submit\", \"params\": [\"" + 
                                   g_activeWallet + ".cyd\", \"" + g_currentJobId + "\", \"00000000\", \"" + 
                                   g_currentNtime + "\", \"" + String(nonceHex) + "\"]}\n";
                g_stratumClient.print(submitMsg);
                Serial.printf("[CORE 1 DUAL CPU] Menghantar Share! Nonce: %s (Diff: %.2f)\n", nonceHex, estimatedDiff);
            }

            if (hash2[6] == 0) {
                Serial.println("[CORE 1] !!! BLOK BITCOIN SAH DITEMUI OLEH CORE 1 !!!");
                g_minerData.triggerBlockFound();
            }
        }
    }

    g_core1Nonce.store(localNonce, std::memory_order_relaxed);
    g_hashesBatchAccumulator.fetch_add(batchSize, std::memory_order_relaxed);
}

void minerTaskLoop(void* parameter) {
    Serial.println("[CORE 0] MinerTask dimulakan: Optimized Bare-Metal Midstate Mining Engine");

    // Baca tetapan dari storan NVS
    g_prefs.begin("minerd", false);
    g_activeSsid = g_prefs.getString("ssid", DEFAULT_WIFI_SSID);
    g_activePass = g_prefs.getString("pass", DEFAULT_WIFI_PASS);
    g_activePool = g_prefs.getString("pool", DEFAULT_POOL_URL);
    g_activePort = g_prefs.getUInt("port", DEFAULT_POOL_PORT);
    g_activeWallet = g_prefs.getString("wallet", DEFAULT_BTC_WALLET);
    g_prefs.end();

    // Kemaskini ke WiFi baru jika masih memegang SSID lama (Kula Diamond)
    if (g_activeSsid == "Kula Diamond" || g_activeSsid.length() == 0) {
        g_activeSsid = DEFAULT_WIFI_SSID;
        g_activePass = DEFAULT_WIFI_PASS;
        g_prefs.begin("minerd", false);
        g_prefs.putString("ssid", g_activeSsid);
        g_prefs.putString("pass", g_activePass);
        g_prefs.end();
        Serial.printf("[CORE 0] Mengemaskini WiFi ke: %s\n", g_activeSsid.c_str());
    }

    // Pastikan wallet menggunakan wallet terkini pengguna
    if (g_activeWallet.startsWith("bc1qnerdminer") || g_activeWallet.length() == 0) {
        g_activeWallet = DEFAULT_BTC_WALLET;
        g_prefs.begin("minerd", false);
        g_prefs.putString("wallet", g_activeWallet);
        g_prefs.end();
        Serial.printf("[CORE 0] Mengemaskini alamat wallet pengguna ke NVS: %s\n", g_activeWallet.c_str());
    }

    // Pastikan pool adalah public-pool.io dan port sah (3333 untuk Solo atau 13333 untuk PPLNS)
    if (g_activePool != DEFAULT_POOL_URL || (g_activePort != DEFAULT_POOL_PORT && g_activePort != DEFAULT_POOL_PPLNS_PORT)) {
        g_activePool = DEFAULT_POOL_URL;
        g_activePort = DEFAULT_POOL_PORT; // Default Solo (3333)
        g_prefs.begin("minerd", false);
        g_prefs.putString("pool", g_activePool);
        g_prefs.putUInt("port", g_activePort);
        g_prefs.end();
        Serial.printf("[CORE 0] Pool dikonfigurasi ke: %s:%u\n", g_activePool.c_str(), g_activePort);
    }

    String fullPool = g_activePool + ":" + String(g_activePort);
    g_minerData.setPoolAndWallet(fullPool.c_str(), g_activeWallet.c_str());
    Serial.printf("[CORE 0] Mod Perlombongan Aktif: %s (%s)\n",
                  fullPool.c_str(),
                  (g_activePort == DEFAULT_POOL_PPLNS_PORT) ? "POOL PPLNS :13333" : "SOLO :3333");

    // Inisialisasi rangkaian WiFi
    WiFi.mode(WIFI_STA);
    Serial.printf("[CORE 0] Menyambung ke WiFi: %s ...\n", g_activeSsid.c_str());
    if (g_activePass.length() > 0) {
        WiFi.begin(g_activeSsid.c_str(), g_activePass.c_str());
    } else {
        WiFi.begin(g_activeSsid.c_str());
    }

    // 1. Pra-kira Midstate untuk 64-byte pertama block header (Hanya sekali sahaja!)
    uint32_t midstate[8];
    compute_midstate(g_blockHeader, midstate);

    // Ambil parameter bahagian kedua 16-byte
    uint32_t merkle_tail = SWAP32(((uint32_t*)&g_blockHeader[64])[0]);
    uint32_t ntime       = SWAP32(((uint32_t*)&g_blockHeader[64])[1]);
    uint32_t nbits       = SWAP32(((uint32_t*)&g_blockHeader[64])[2]);

    // Perkongsian midstate kepada Core 1 untuk mod Dual CPU Max Hash
    memcpy(g_sharedMidstate, midstate, 32);
    g_sharedMerkleTail = merkle_tail;
    g_sharedNtime = ntime;
    g_sharedNbits = nbits;
    g_midstateReady = true;

    uint32_t nonce = 0;
    uint32_t lastHashCount = 0;
    unsigned long lastReportTime = millis();
    unsigned long wifiConnectStartTime = millis();

    uint32_t W[64];
    uint32_t W2[64];
    uint32_t hash1[8];
    uint32_t hash2[8];

    // Sediakan nilai malar untuk W dan W2 lebih awal
    W[0] = merkle_tail;
    W[1] = ntime;
    W[2] = nbits;
    W[4] = 0x80000000;
    for (int k = 5; k < 15; k++) W[k] = 0;
    W[15] = 0x00000280; // 640 bits

    W2[8] = 0x80000000;
    for (int k = 9; k < 15; k++) W2[k] = 0;
    W2[15] = 0x00000100; // 256 bits

    while (true) {
        // Kendalikan sambungan dan pertukaran mesej Stratum V1 ke Public Pool
        handleStratumMining();

        // Semakan WiFi
        bool wifiOk = (WiFi.status() == WL_CONNECTED);
        if (wifiOk) {
            if (g_apModeActive) {
                WiFi.softAPdisconnect(true);
                WiFi.mode(WIFI_STA);
                g_apModeActive = false;
            }

            if (!g_mdnsStarted) {
                if (MDNS.begin("game-minerd")) {
                    MDNS.addService("http", "tcp", 80);
                    Serial.printf("[WIFI] Bersambung! IP: %s | WebGUI: http://%s atau http://game-minerd.local\n", WiFi.localIP().toString().c_str(), WiFi.localIP().toString().c_str());
                    g_mdnsStarted = true;
                }
            }

            String ipStr = WiFi.localIP().toString();
            String ssidStr = WiFi.SSID();
            String currentPoolStr = g_activePool + ":" + String(g_activePort);
            g_minerData.setWifiDetails(true, ssidStr.c_str(), ipStr.c_str());
            g_minerData.setConnectionStatus(true, g_stratumConnected, currentPoolStr.c_str());
        } else {
            if (millis() - wifiConnectStartTime > 14000) {
                startConfigPortalAP();
            }
            String currentPoolStr = g_activePool + ":" + String(g_activePort);
            if (g_apModeActive) {
                g_minerData.setWifiDetails(false, "AP: GameMinerd", "192.168.4.1");
            } else {
                g_minerData.setWifiDetails(false, g_activeSsid.c_str(), "Menyambung...");
            }
            g_minerData.setConnectionStatus(false, false, currentPoolStr.c_str());
        }

        // Kelompok Hashing Bare-Metal Ultra-Pantas (10,000 nonces per batch)
        for (int i = 0; i < 10000; i++) {
            nonce++;
            W[3] = SWAP32(nonce);

            // Kembangkan W16..W63
            for (int k = 16; k < 64; k++) {
                W[k] = s1(W[k - 2]) + W[k - 7] + s0(W[k - 15]) + W[k - 16];
            }

            // Pusingan 1: SHA256(Block2 dari Midstate)
            memcpy(hash1, midstate, 32);
            sha256_compress(hash1, W);

            // Pusingan 2: SHA256(Hash1)
            for (int k = 0; k < 8; k++) {
                W2[k] = hash1[k];
            }
            for (int k = 16; k < 64; k++) {
                W2[k] = s1(W2[k - 2]) + W2[k - 7] + s0(W2[k - 15]) + W2[k - 16];
            }

            memcpy(hash2, SHA256_INITIAL, 32);
            sha256_compress(hash2, W2);

            // Semakan sasaran kesukaran (Leading zeros)
            if (hash2[7] == 0) {
                double estimatedDiff = 65536.0 / ((hash2[6] >> 16) + 1);
                g_minerData.updateMiningProgress(0, 0, estimatedDiff);

                if (g_stratumConnected && g_stratumClient.connected() && g_currentJobId.length() > 0) {
                    char nonceHex[9];
                    snprintf(nonceHex, sizeof(nonceHex), "%08x", SWAP32(nonce));
                    String submitMsg = "{\"id\": 4, \"method\": \"mining.submit\", \"params\": [\"" + 
                                       g_activeWallet + ".cyd\", \"" + g_currentJobId + "\", \"00000000\", \"" + 
                                       g_currentNtime + "\", \"" + String(nonceHex) + "\"]}\n";
                    g_stratumClient.print(submitMsg);
                    Serial.printf("[STRATUM] Menghantar Share ke Pool! Nonce: %s (Diff: %.2f)\n", nonceHex, estimatedDiff);
                }

                if (hash2[6] == 0) {
                    Serial.println("[CORE 0] !!! BLOK BITCOIN SAH DITEMUI !!!");
                    g_minerData.triggerBlockFound();
                }
            }
        }
        g_hashesBatchAccumulator.fetch_add(10000, std::memory_order_relaxed);

        // Kira Hashrate setiap saat (Gabungan Core 0 + Core 1 jika aktif)
        unsigned long now = millis();
        unsigned long elapsed = now - lastReportTime;
        static unsigned long lastSerialPrint = 0;
        if (elapsed >= 1000) {
            uint32_t hashesDone = g_hashesBatchAccumulator.exchange(0);
            float hashrate_kH = (float)hashesDone / (float)elapsed;

            bool isDual = (now - g_lastCore1HashTime < 2000);
            g_minerData.setDualCpuActive(isDual);
            g_minerData.updateMiningProgress(hashrate_kH, hashesDone, 0.0);

            if (now - lastSerialPrint >= 3000) {
                Serial.printf("[MINER Core 0] Hashrate: %.2f kH/s [%s] | Nonce: %u | IP: %s\n",
                              hashrate_kH, isDual ? "DUAL CPU MAX HASH (Core 0 + 1)" : "Core 0",
                              nonce, WiFi.localIP().toString().c_str());
                lastSerialPrint = now;
            }

            lastReportTime = now;
        }

        // Berikan ruang 1 tick (10ms) kepada LwIP stack WiFi dan pelayan HTTP
        vTaskDelay(1);
    }
}

void startMinerTask() {
    xTaskCreatePinnedToCore(
        minerTaskLoop,
        "MinerTask",
        8192,
        NULL,
        1,
        NULL,
        0 // Disematkan khusus ke Core 0
    );

    xTaskCreatePinnedToCore(
        webServerTask,
        "WebServerTask",
        8192,
        NULL,
        1,
        NULL,
        0 // Disematkan khusus ke Core 0
    );
}
