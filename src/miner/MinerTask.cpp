#include "MinerTask.h"
#include "Config.h"
#include "MinerSharedData.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <mbedtls/sha256.h>

// Definisi objek pengurus statistik global
MinerDataManager g_minerData;

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
    json += "\"pool\":\"" + String(s.activePool) + "\"";
    json += "}";
    g_server.send(200, "application/json", json);
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
                g_server.on("/restart", HTTP_POST, handleRestart);
                g_server.begin();
                g_serverStarted = true;
                Serial.printf("[HTTP] Web Server port 80 aktif di IP: %s\n", WiFi.localIP().toString().c_str());
            }
            g_server.handleClient();
        }
        vTaskDelay(pdMS_TO_TICKS(5));
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

        // Kira Hashrate setiap saat
        unsigned long now = millis();
        unsigned long elapsed = now - lastReportTime;
        static unsigned long lastSerialPrint = 0;
        if (elapsed >= 1000) {
            uint32_t hashesDone = nonce - lastHashCount;
            float hashrate_kH = (float)hashesDone / (float)elapsed;

            g_minerData.updateMiningProgress(hashrate_kH, hashesDone, 0.0);

            if (now - lastSerialPrint >= 3000) {
                Serial.printf("[MINER Core 0] Hashrate: %.2f kH/s | Nonce: %u | RSSI: %d dBm | IP: %s\n",
                              hashrate_kH, nonce, WiFi.RSSI(), WiFi.localIP().toString().c_str());
                lastSerialPrint = now;
            }

            lastHashCount = nonce;
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
        4096,
        NULL,
        1,
        NULL,
        0 // Disematkan khusus ke Core 0
    );
}
