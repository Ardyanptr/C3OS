#include "update.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <esp_task_wdt.h>
#include <mbedtls/md.h>

#include "icons/icon.h"

// ─────────────────────────────────────────────────────────────
//  Constants
// ─────────────────────────────────────────────────────────────
static const char *MANIFEST_URL =
    "https://raw.githubusercontent.com/Ardyanptr/C3OS/refs/heads/main/data/cfg/update.json";

static const char *LOCAL_CFG = "/cfg/update.json";
static const char *PENDING_FILE = "/cfg/pending_update.json";
static const char *TMP_DIR = "/tmp";

static const uint32_t HTTP_TIMEOUT_MS = 15000;
static const uint32_t WIFI_PORTAL_TIMEOUT = 30;
static const size_t CHUNK_SIZE = 512;

// ─────────────────────────────────────────────────────────────
//  Static state
//  OneButton only accepts plain void(*)() — no lambda captures.
//  We store everything we need in module-level statics instead.
// ─────────────────────────────────────────────────────────────
static bool updater_Running = false;
static String s_remoteVer = "";
static DynamicJsonDocument s_remoteDoc(4096);

// ─────────────────────────────────────────────────────────────
//  Semver
// ─────────────────────────────────────────────────────────────
struct SemVer {
    int major, minor, patch;
};

static SemVer parseSemVer(const String &s) {
    SemVer v = {0, 0, 0};
    sscanf(s.c_str(), "%d.%d.%d", &v.major, &v.minor, &v.patch);
    return v;
}
static bool semverNewer(const SemVer &r, const SemVer &l) {
    if (r.major != l.major) return r.major > l.major;
    if (r.minor != l.minor) return r.minor > l.minor;
    return r.patch > l.patch;
}

// ─────────────────────────────────────────────────────────────
//  SHA256
// ─────────────────────────────────────────────────────────────
static String sha256File(const String &path) {
    File f = LittleFS.open(path, "r");
    if (!f) return "";

    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    mbedtls_md_starts(&ctx);

    uint8_t buf[CHUNK_SIZE];
    while (f.available()) {
        size_t n = f.read(buf, sizeof(buf));
        mbedtls_md_update(&ctx, buf, n);
        esp_task_wdt_reset();
    }
    f.close();

    uint8_t hash[32];
    mbedtls_md_finish(&ctx, hash);
    mbedtls_md_free(&ctx);

    String hex;
    hex.reserve(64);
    for (int i = 0; i < 32; i++) {
        char b[3];
        snprintf(b, sizeof(b), "%02x", hash[i]);
        hex += b;
    }
    return hex;
}

// ─────────────────────────────────────────────────────────────
//  FS helpers
// ─────────────────────────────────────────────────────────────
static void ensureTmpDir() {
    if (!LittleFS.exists(TMP_DIR)) LittleFS.mkdir(TMP_DIR);
}

static bool commitStaging(const String &staging, const String &final_) {
    if (LittleFS.exists(final_)) LittleFS.remove(final_);
    return LittleFS.rename(staging, final_);
}

// ─────────────────────────────────────────────────────────────
//  Download → staging
//  - WDT reset every chunk
//  - hard timeout guard (no data for N ms = abort)
//  - optional progress callback(bytesWritten, contentLength)
// ─────────────────────────────────────────────────────────────
static const uint32_t STALL_TIMEOUT_MS = 8000; // abort if no data for 8s

static int32_t downloadToStaging(WiFiClientSecure &client, const String &url,
                                 const String &stagingPath, size_t maxBytes = 256 * 1024,
                                 void (*onProgress)(int32_t, int32_t) = nullptr) {
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setReuse(false);

    if (!http.begin(client, url)) {
        Serial.println("[DL] begin failed");
        return -1;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[DL] HTTP %d\n", code);
        http.end();
        return -1;
    }

    int32_t contentLen = http.getSize(); // -1 if chunked, that's fine

    File f = LittleFS.open(stagingPath, "w");
    if (!f) {
        Serial.println("[DL] cannot open staging");
        http.end();
        return -1;
    }

    WiFiClient *stream = http.getStreamPtr();
    uint8_t buf[CHUNK_SIZE];
    int32_t total = 0;
    uint32_t lastData = millis();

    while (true) {
        // stall guard — no data for STALL_TIMEOUT_MS → abort
        if (millis() - lastData > STALL_TIMEOUT_MS) {
            Serial.println("[DL] stall timeout");
            f.close();
            http.end();
            LittleFS.remove(stagingPath);
            return -1;
        }

        // done condition
        if (!http.connected() && !stream->available()) break;
        if (contentLen > 0 && total >= contentLen) break;

        size_t avail = stream->available();
        if (!avail) {
            // yield to RTOS + reset WDT while waiting
            esp_task_wdt_reset();
            vTaskDelay(5 / portTICK_PERIOD_MS);
            continue;
        }

        size_t toRead = min(avail, sizeof(buf));
        size_t n = stream->readBytes(buf, toRead);
        if (!n) break;

        f.write(buf, n);
        total += (int32_t)n;
        lastData = millis();

        // WDT reset every chunk — critical
        esp_task_wdt_reset();

        // size guard
        if ((size_t)total > maxBytes) {
            Serial.println("[DL] file too large");
            f.close();
            http.end();
            LittleFS.remove(stagingPath);
            return -1;
        }

        // live progress callback
        if (onProgress) onProgress(total, contentLen);
    }

    f.close();
    http.end();

    if (!total) {
        LittleFS.remove(stagingPath);
        return -1;
    }

    Serial.printf("[DL] done: %d bytes -> %s\n", total, stagingPath.c_str());
    return total;
}

// ─────────────────────────────────────────────────────────────
//  UI helpers — modern, centred, no ornaments
// ─────────────────────────────────────────────────────────────
static void uiRule(uint8_t y) { display.drawHLine(16, y, 96); }

static void uiStatus(const char *title, const char *sub = nullptr) {
    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);
    display.setFont(u8g2_font_helvB08_tr);
    display.drawStr((128 - display.getStrWidth(title)) / 2, 26, title);
    if (sub) {
        display.setFont(u8g2_font_4x6_tr);
        display.drawStr((128 - display.getStrWidth(sub)) / 2, 38, sub);
    }
    uiRule(45);
    display.sendBuffer();
}

// step/total = file index, bytesNow/bytesTotal = live byte progress within file
static void uiStageProgress(const char *filename, uint8_t step, uint8_t total, int32_t bytesNow = 0,
                            int32_t bytesTotal = -1) {
    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    display.setFont(u8g2_font_4x6_tr);
    char buf[28];
    snprintf(buf, sizeof(buf), "STAGING  %d / %d", step + 1, total);
    display.drawStr((128 - display.getStrWidth(buf)) / 2, 10, buf);
    uiRule(14);

    char fname[22];
    strncpy(fname, filename, 21);
    fname[21] = '\0';
    display.drawStr(8, 24, fname);

    // byte-level bar
    const uint8_t BX = 8, BY = 33, BW = 112;
    display.drawHLine(BX, BY, BW);

    uint8_t fillW = 0;
    if (bytesTotal > 0) {
        fillW = (uint8_t)((float)bytesNow / bytesTotal * BW);
    } else if (bytesNow > 0) {
        // chunked transfer — bounce fill so it doesn't look frozen
        fillW = (uint8_t)((bytesNow / 512) % (BW + 1));
    }
    if (fillW > BW) fillW = BW;
    display.drawBox(BX, BY, fillW, 2);

    // byte counter
    if (bytesNow > 0) {
        if (bytesTotal > 0)
            snprintf(buf, sizeof(buf), "%ld / %ld B", (long)bytesNow, (long)bytesTotal);
        else
            snprintf(buf, sizeof(buf), "%ld B recv", (long)bytesNow);
        display.drawStr((128 - display.getStrWidth(buf)) / 2, 46, buf);
    }

    // file-level dot row
    uint8_t dotN = total > 8 ? 8 : total;
    uint8_t startX = (128 - dotN * 8) / 2;
    for (uint8_t d = 0; d < dotN; d++) {
        uint8_t dx = startX + d * 8;
        if (d < step)
            display.drawBox(dx, 57, 4, 2);
        else if (d == step)
            display.drawFrame(dx, 57, 4, 2);
    }

    display.sendBuffer();
}

static void uiError(const char *msg) {
    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    display.setFont(u8g2_font_helvB08_tr);
    const char *t = "UPDATE FAILED";
    display.drawStr((128 - display.getStrWidth(t)) / 2, 14, t);
    uiRule(18);

    display.setFont(u8g2_font_4x6_tr);
    display.drawStr((128 - display.getStrWidth(msg)) / 2, 30, msg);
    const char *n = "No changes applied.";
    display.drawStr((128 - display.getStrWidth(n)) / 2, 40, n);
    uiRule(46);

    display.drawRFrame(44, 52, 40, 10, 2);
    const char *ok = "OK";
    display.drawStr(44 + (40 - display.getStrWidth(ok)) / 2, 60, ok);
    display.sendBuffer();

    btnOK.attachClick([]() { updater_Running = false; });
    btnOK.attachLongPressStart(nullptr);
}

static void uiWiFiError() {
    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    display.setFont(u8g2_font_helvB08_tr);
    const char *t = "NO CONNECTION";
    display.drawStr((128 - display.getStrWidth(t)) / 2, 14, t);
    uiRule(18);

    display.setFont(u8g2_font_4x6_tr);
    const char *l1 = "Could not reach server.";
    const char *l2 = "Check WiFi, try again.";
    display.drawStr((128 - display.getStrWidth(l1)) / 2, 30, l1);
    display.drawStr((128 - display.getStrWidth(l2)) / 2, 40, l2);
    uiRule(46);

    display.drawRFrame(44, 52, 40, 10, 2);
    const char *ok = "OK";
    display.drawStr(44 + (40 - display.getStrWidth(ok)) / 2, 60, ok);
    display.sendBuffer();

    btnOK.attachClick([]() { updater_Running = false; });
    btnOK.attachLongPressStart(nullptr);
}

// ─────────────────────────────────────────────────────────────
//  Circular spinner + dot progress — used during boot apply
// ─────────────────────────────────────────────────────────────
static void uiDrawSpinner(uint8_t angle) {
    const int8_t CX = 64, CY = 32, R = 10;
    const float STEP = 6.2832f / 8.0f;
    for (uint8_t i = 0; i < 8; i++) {
        float a = i * STEP;
        int8_t x = CX + (int8_t)(cosf(a) * R);
        int8_t y = CY + (int8_t)(sinf(a) * R);
        uint8_t slot = (i + angle) % 8;
        if (slot == 0) {
            // head — 2x2 bright
            display.drawBox(x, y, 2, 2);
        } else if (slot < 4) {
            // tail — single pixel
            display.drawPixel(x, y);
        }
        // slot 4-7: invisible gap
    }
}

static void uiApplyScreen(uint8_t step, uint8_t total, uint8_t spinAngle) {
    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    uiDrawSpinner(spinAngle);

    display.setFont(u8g2_font_4x6_tr);
    const char *label = (step >= total) ? "Done" : "Applying update...";
    display.drawStr((128 - display.getStrWidth(label)) / 2, 52, label);

    // dot row — 5 dots centred
    const uint8_t DOT_N = 5;
    uint8_t lit = (total > 0) ? (uint8_t)((float)step / total * DOT_N + 0.5f) : 0;
    for (uint8_t d = 0; d < DOT_N; d++) {
        uint8_t dx = 49 + d * 8;
        if (d < lit)
            display.drawBox(dx, 58, 4, 2);
        else
            display.drawFrame(dx, 58, 4, 2);
    }

    display.sendBuffer();
}

// ─────────────────────────────────────────────────────────────
//  Stage all files
// ─────────────────────────────────────────────────────────────
// module-level context for progress callback (no lambda capture needed)
static const char *s_progressFilename = "";
static uint8_t s_progressStep = 0;
static uint8_t s_progressTotal = 0;

static void onDownloadProgress(int32_t now, int32_t total) {
    uiStageProgress(s_progressFilename, s_progressStep, s_progressTotal, now, total);
    // WDT reset inside callback so even slow UI draws don't starve it
    esp_task_wdt_reset();
}

static bool stageAllFiles(WiFiClientSecure &client, JsonArray &fileList, uint8_t total) {
    ensureTmpDir();
    uint8_t idx = 0;

    for (JsonObject item : fileList) {
        const char *lp = item["local_path"] | "";
        const char *url = item["url"] | "";
        const char *sha = item["sha256"] | "";

        if (!strlen(lp) || !strlen(url)) {
            idx++;
            continue;
        }

        String sp = String(TMP_DIR) + "/upd_" + idx;

        // set context for static callback
        s_progressFilename = lp;
        s_progressStep = idx;
        s_progressTotal = total;

        // draw initial state before bytes arrive
        uiStageProgress(lp, idx, total, 0, -1);
        esp_task_wdt_reset();

        if (downloadToStaging(client, url, sp, 256 * 1024, onDownloadProgress) < 0) {
            uiError("Download failed.");
            for (uint8_t j = 0; j <= idx; j++)
                LittleFS.remove(String(TMP_DIR) + "/upd_" + j);
            return false;
        }

        // verify hash
        if (strlen(sha) == 64) {
            uiStatus("Verifying...", lp);
            esp_task_wdt_reset();
            if (!sha256File(sp).equalsIgnoreCase(sha)) {
                uiError("Hash mismatch!");
                for (uint8_t j = 0; j <= idx; j++)
                    LittleFS.remove(String(TMP_DIR) + "/upd_" + j);
                return false;
            }
        }

        idx++;
        esp_task_wdt_reset();
    }
    return true;
}

// ─────────────────────────────────────────────────────────────
//  Write pending manifest
// ─────────────────────────────────────────────────────────────
static bool writePendingManifest(JsonArray &fileList) {
    DynamicJsonDocument pending(4096);
    JsonArray arr = pending.createNestedArray("files");

    uint8_t idx = 0;
    for (JsonObject item : fileList) {
        const char *lp = item["local_path"] | "";
        if (!strlen(lp)) {
            idx++;
            continue;
        }
        JsonObject e = arr.createNestedObject();
        e["staging"] = String(TMP_DIR) + "/upd_" + idx;
        e["final"] = lp;
        idx++;
    }
    if (s_remoteDoc.containsKey("updates_config")) {
        JsonObject e = arr.createNestedObject();
        e["staging"] = String(TMP_DIR) + "/upd_cfg";
        e["final"] = LOCAL_CFG;
    }

    File f = LittleFS.open(PENDING_FILE, "w");
    if (!f) return false;
    serializeJson(pending, f);
    f.close();
    return true;
}

// ─────────────────────────────────────────────────────────────
//  Install callback — plain function, reads from statics
// ─────────────────────────────────────────────────────────────
static void onInstallPressed() {
    uiStatus("Staging...", "Do not power off");
    esp_task_wdt_reset();

    WiFiClientSecure sc;
    sc.setInsecure();

    JsonArray fileList = s_remoteDoc["updates_file"].as<JsonArray>();
    uint8_t total = 0;
    for (auto _ : fileList)
        total++;

    if (!stageAllFiles(sc, fileList, total)) return;

    if (s_remoteDoc.containsKey("updates_config")) {
        const char *cfgUrl = s_remoteDoc["updates_config"]["url"] | "";
        if (strlen(cfgUrl)) {
            uiStatus("Staging config...");
            if (downloadToStaging(sc, cfgUrl, String(TMP_DIR) + "/upd_cfg", 8192) < 0) {
                uiError("Config download failed.");
                return;
            }
        }
    }

    if (!writePendingManifest(fileList)) {
        uiError("Cannot write pending file.");
        return;
    }

    // Success — reboot prompt
    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);

    display.setFont(u8g2_font_helvB08_tr);
    const char *title = "READY TO INSTALL";
    display.drawStr((128 - display.getStrWidth(title)) / 2, 12, title);
    uiRule(16);

    display.setFont(u8g2_font_4x6_tr);
    String vl = "v" + s_remoteVer + " staged";
    display.drawStr((128 - display.getStrWidth(vl.c_str())) / 2, 28, vl.c_str());
    const char *note = "Will apply on next boot.";
    display.drawStr((128 - display.getStrWidth(note)) / 2, 38, note);
    uiRule(44);

    display.drawRFrame(4, 52, 56, 10, 2);
    display.drawRFrame(68, 52, 56, 10, 2);
    const char *rb = "Reboot Now";
    const char *lt = "Later";
    display.drawStr(4 + (56 - display.getStrWidth(rb)) / 2, 60, rb);
    display.drawStr(68 + (56 - display.getStrWidth(lt)) / 2, 60, lt);
    display.sendBuffer();

    btnOK.attachClick([]() { esp_restart(); });
    btnOK.attachLongPressStart([]() { updater_Running = false; });
}

// ─────────────────────────────────────────────────────────────
//  Homepage
// ─────────────────────────────────────────────────────────────
static void updateHomepage() {
    uiStatus("Checking...");

    String localVer = "0.0.0";
    {
        File lf = LittleFS.open(LOCAL_CFG, "r");
        if (lf) {
            StaticJsonDocument<256> ld;
            if (!deserializeJson(ld, lf)) localVer = ld["ver"] | "0.0.0";
            lf.close();
        }
    }

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.begin(client, MANIFEST_URL);

    if (http.GET() != HTTP_CODE_OK) {
        http.end();
        uiWiFiError();
        return;
    }

    s_remoteDoc.clear();
    if (deserializeJson(s_remoteDoc, http.getStream())) {
        http.end();
        uiError("Bad manifest.");
        return;
    }
    http.end();

    s_remoteVer = s_remoteDoc["ver"] | "0.0.0";

    if (!semverNewer(parseSemVer(s_remoteVer), parseSemVer(localVer))) {
        display.clearBuffer();
        display.setFontMode(1);
        display.setBitmapMode(1);
        display.setFont(u8g2_font_helvB08_tr);
        const char *t = "UP TO DATE";
        display.drawStr((128 - display.getStrWidth(t)) / 2, 14, t);
        uiRule(18);
        display.setFont(u8g2_font_4x6_tr);
        String cv = "v" + localVer;
        display.drawStr((128 - display.getStrWidth(cv.c_str())) / 2, 30, cv.c_str());
        const char *n = "No action needed.";
        display.drawStr((128 - display.getStrWidth(n)) / 2, 40, n);
        uiRule(46);
        display.drawRFrame(44, 52, 40, 10, 2);
        const char *ok = "OK";
        display.drawStr(44 + (40 - display.getStrWidth(ok)) / 2, 60, ok);
        display.sendBuffer();
        btnOK.attachClick([]() { updater_Running = false; });
        btnOK.attachLongPressStart(nullptr);
        return;
    }

    // Update available
    display.clearBuffer();
    display.setFontMode(1);
    display.setBitmapMode(1);
    display.setFont(u8g2_font_helvB08_tr);
    const char *t = "UPDATE AVAILABLE";
    display.drawStr((128 - display.getStrWidth(t)) / 2, 12, t);
    uiRule(16);
    display.setFont(u8g2_font_4x6_tr);
    String nv = "New     v" + s_remoteVer;
    String ov = "Current v" + localVer;
    display.drawStr((128 - display.getStrWidth(nv.c_str())) / 2, 26, nv.c_str());
    display.drawStr((128 - display.getStrWidth(ov.c_str())) / 2, 35, ov.c_str());
    const char *hint = "Hold OK to skip";
    display.drawStr((128 - display.getStrWidth(hint)) / 2, 44, hint);
    uiRule(47);
    display.drawRFrame(4, 52, 56, 10, 2);
    display.drawRFrame(68, 52, 56, 10, 2);
    const char *inst = "INSTALL";
    const char *skip = "Skip";
    display.drawStr(4 + (56 - display.getStrWidth(inst)) / 2, 60, inst);
    display.drawStr(68 + (56 - display.getStrWidth(skip)) / 2, 60, skip);
    display.sendBuffer();

    btnOK.attachClick(onInstallPressed);
    btnOK.attachLongPressStart([]() { updater_Running = false; });
}

// ─────────────────────────────────────────────────────────────
//  Boot-time apply with spinner UI
// ─────────────────────────────────────────────────────────────
void applyPendingUpdate() {
    if (!LittleFS.exists(PENDING_FILE)) return;

    File f = LittleFS.open(PENDING_FILE, "r");
    if (!f) return;

    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc, f)) {
        f.close();
        LittleFS.remove(PENDING_FILE);
        return;
    }
    f.close();

    JsonArray files = doc["files"].as<JsonArray>();
    uint8_t total = 0;
    for (auto _ : files)
        total++;

    uint8_t step = 0, spinAngle = 0;

    for (JsonObject item : files) {
        String staging = item["staging"] | "";
        String final_ = item["final"] | "";

        uiApplyScreen(step, total, spinAngle);
        spinAngle = (spinAngle + 1) % 8;

        if (!staging.isEmpty() && !final_.isEmpty() && LittleFS.exists(staging)) {
            int slash = final_.lastIndexOf('/');
            if (slash > 0) {
                String dir = final_.substring(0, slash);
                if (!LittleFS.exists(dir)) LittleFS.mkdir(dir);
            }
            commitStaging(staging, final_);
        }

        esp_task_wdt_reset();
        step++;
        delay(80);
    }

    LittleFS.remove(PENDING_FILE);

    uiApplyScreen(total, total, spinAngle);
    delay(800);

    display.clearBuffer();
    display.sendBuffer();
}

// ─────────────────────────────────────────────────────────────
//  Entry point
// ─────────────────────────────────────────────────────────────
void proceedUpdate() {
    updater_Running = true;
    btnOK.attachClick(nullptr);
    btnOK.attachLongPressStart(nullptr);

    uiStatus("Connecting...", "Please wait");
    esp_task_wdt_reset();

    WiFiManager wm;
    wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT);
    wm.setConnectTimeout(15);

    if (!wm.autoConnect("C3OS-Update", "123456789"))
        uiWiFiError();
    else
        updateHomepage();

    while (updater_Running) {
        btnOK.tick();
        vTaskDelay(10 / portTICK_PERIOD_MS);
        esp_task_wdt_reset();
    }

    btnOK.attachClick(nullptr);
    btnOK.attachLongPressStart(nullptr);
    s_remoteDoc.clear();

    drawMenu();
}