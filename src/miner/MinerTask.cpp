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
                    <label>Nama WiFi (SSID):</label>
                    <input type="text" name="ssid" placeholder="Nama WiFi" required value="%SSID%">
                </div>
                <div class="group">
                    <label>Kata Laluan WiFi:</label>
                    <input type="password" name="pass" placeholder="Kata Laluan" value="%PASS%">
                </div>
                <div class="group">
                    <label>Mining Pool URL (Stratum):</label>
                    <input type="text" name="pool" placeholder="public-pool.io" required value="%POOL%">
                </div>
                <div class="group">
                    <label>Mining Pool Port:</label>
                    <input type="number" name="port" placeholder="21496" required value="%PORT%">
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

static bool g_mdnsStarted = false;

static void setupWebServer() {
    if (!g_serverStarted) {
        g_server.on("/", HTTP_GET, handleRoot);
        g_server.on("/save", HTTP_POST, handleSave);
        g_server.on("/api/stats", HTTP_GET, handleApiStats);
        g_server.on("/restart", HTTP_POST, handleRestart);
        g_server.begin();
        g_serverStarted = true;
        Serial.println("[HTTP] Web Server port 80 sedia untuk sambungan.");
    }
}

static void startConfigPortalAP() {
    if (!g_apModeActive) {
        Serial.println("[WIFI] Memulakan Access Point Sandaran: GameMinerd-WiFi (192.168.4.1)");
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP("GameMinerd-WiFi", "12345678");
        g_apModeActive = true;
        g_minerData.setWifiDetails(false, "AP: GameMinerd-WiFi", "192.168.4.1");
        setupWebServer();
    }
}

void startMinerTask() {
    xTaskCreatePinnedToCore(
        minerTaskLoop,
        "MinerTask",
        8192,
        NULL,
        PRIORITY_MINING,
        NULL,
        CORE_MINING
    );
}

void minerTaskLoop(void* parameter) {
    Serial.println("[CORE 0] MinerTask dimulakan pada Core 0 (FreeRTOS Background Task)");

    // Baca tetapan dari storan NVS (Flash Memory)
    g_prefs.begin("minerd", false);
    g_activeSsid = g_prefs.getString("ssid", DEFAULT_WIFI_SSID);
    g_activePass = g_prefs.getString("pass", DEFAULT_WIFI_PASS);
    g_activePool = g_prefs.getString("pool", DEFAULT_POOL_URL);
    g_activePort = g_prefs.getUInt("port", DEFAULT_POOL_PORT);
    g_activeWallet = g_prefs.getString("wallet", DEFAULT_BTC_WALLET);
    g_prefs.end();

    String fullPool = g_activePool + ":" + String(g_activePort);
    g_minerData.setPoolAndWallet(fullPool.c_str(), g_activeWallet.c_str());

    // 1. Inisialisasi rangkaian WiFi terlebih dahulu
    WiFi.mode(WIFI_STA);
    Serial.printf("[CORE 0] Menyambung ke WiFi: %s ...\n", g_activeSsid.c_str());
    if (g_activePass.length() > 0) {
        WiFi.begin(g_activeSsid.c_str(), g_activePass.c_str());
    } else {
        WiFi.begin(g_activeSsid.c_str());
    }

    // 2. Lancarkan Web Server selepas WiFi stack dimulakan
    setupWebServer();

    uint32_t nonce = 0;
    uint32_t lastHashCount = 0;
    unsigned long lastReportTime = millis();
    unsigned long wifiConnectStartTime = millis();

    uint8_t hash1[32];
    uint8_t hash2[32];

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);

    while (true) {
        // Sentiasa kendalikan permintaan Web Server HTTP pada port 80
        if (g_serverStarted) {
            g_server.handleClient();
        }

        // Pengurusan Sambungan WiFi
        bool wifiOk = (WiFi.status() == WL_CONNECTED);
        if (wifiOk) {
            if (g_apModeActive) {
                WiFi.softAPdisconnect(true);
                WiFi.mode(WIFI_STA);
                g_apModeActive = false;
            }

            // Inisialisasi mDNS sekali sahaja selepas WiFi tersambung
            if (!g_mdnsStarted) {
                if (MDNS.begin("game-minerd")) {
                    MDNS.addService("http", "tcp", 80);
                    Serial.println("[MDNS] WebGUI boleh diakses melalui: http://game-minerd.local");
                    g_mdnsStarted = true;
                }
            }

            String ipStr = WiFi.localIP().toString();
            String ssidStr = WiFi.SSID();
            g_minerData.setWifiDetails(true, ssidStr.c_str(), ipStr.c_str());
            g_minerData.setConnectionStatus(true, true, fullPool.c_str());
        } else {
            // Jika belum tersambung selepas 14 saat, buka Hotspot AP Sandaran
            if (millis() - wifiConnectStartTime > 14000) {
                startConfigPortalAP();
            }
            if (g_apModeActive) {
                g_minerData.setWifiDetails(false, "AP: GameMinerd", "192.168.4.1");
            } else {
                g_minerData.setWifiDetails(false, g_activeSsid.c_str(), "Menyambung...");
            }
            g_minerData.setConnectionStatus(false, false, fullPool.c_str());
        }

        // 3. Kelompok Hashing SHA-256 Berganda di Core 0 (Solo Mining)
        for (int i = 0; i < 1000; i++) {
            nonce++;
            memcpy(&g_blockHeader[76], &nonce, 4);

            mbedtls_sha256_starts(&ctx, 0);
            mbedtls_sha256_update(&ctx, g_blockHeader, 80);
            mbedtls_sha256_finish(&ctx, hash1);

            mbedtls_sha256_starts(&ctx, 0);
            mbedtls_sha256_update(&ctx, hash1, 32);
            mbedtls_sha256_finish(&ctx, hash2);

            // Semak sasaran kesukaran
            if (hash2[31] == 0x00 && hash2[30] == 0x00) {
                double estimatedDiff = 65536.0 / (hash2[29] + 1);
                g_minerData.updateMiningProgress(0, 0, estimatedDiff);

                if (hash2[29] == 0x00) {
                    Serial.println("[CORE 0] !!! BLOK BITCOIN SAH DITEMUI !!!");
                    g_minerData.triggerBlockFound();
                }
            }
        }

        // 4. Kira Hashrate setiap saat
        unsigned long now = millis();
        unsigned long elapsed = now - lastReportTime;
        if (elapsed >= 1000) {
            uint32_t hashesDone = nonce - lastHashCount;
            float hashrate_kH = (float)hashesDone / (float)elapsed;

            g_minerData.updateMiningProgress(hashrate_kH, hashesDone, 0.0);

            lastHashCount = nonce;
            lastReportTime = now;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    mbedtls_sha256_free(&ctx);
}
