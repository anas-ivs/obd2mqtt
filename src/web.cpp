#include "web.h"
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <memory>
#include <utility>
#include <vector>
#include "config.h"
#include "led.h"
#include "log.h"
#include "poller.h"
#include "online_update.h"
#include "web_assets.h"   // generated from web_ui.h / help_ui.h by scripts/gen_web_assets.py
#include "watchdog.h"

namespace Web {

static AsyncWebServer server(80);
static volatile uint32_t rebootAt = 0;
static volatile uint8_t otaState = 0;           // 0 idle, 1 running, 2 done, 3 error
static volatile size_t otaWritten = 0, otaTotal = 0;
static String otaError;

static bool auth(AsyncWebServerRequest* r) {
  if (cfg.webPass.isEmpty()) return true;
  if (r->authenticate("admin", cfg.webPass.c_str())) return true;
  r->requestAuthentication("OBD2MQTT");
  return false;
}

static void sendJson(AsyncWebServerRequest* r, JsonDocument& d, int code = 200) {
  String s;
  serializeJson(d, s);
  r->send(code, "application/json", s);
}

static void sendMsg(AsyncWebServerRequest* r, bool ok, const String& msg, int code = 0) {
  JsonDocument d;
  d["ok"] = ok;
  d["msg"] = msg;
  sendJson(r, d, code ? code : (ok ? 200 : 400));
}

// Body collector: stores the request body as a C string in _tempObject (freed by the server)
static void collectBody(AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t index, size_t total) {
  if (total > 65536) return;   // max. 64 KB (backup file)
  if (index == 0) {
    r->_tempObject = malloc(total + 1);
    if (!r->_tempObject) return;
  }
  if (!r->_tempObject) return;
  memcpy((uint8_t*)r->_tempObject + index, data, len);
  if (index + len == total) ((char*)r->_tempObject)[total] = 0;
}
static String body(AsyncWebServerRequest* r) {
  return r->_tempObject ? String((const char*)r->_tempObject) : String();
}

static void postJson(const char* path, ArRequestHandlerFunction fn) {
  server.on(path, HTTP_POST, fn, nullptr, collectBody);
}

// Static page from flash, gzip-compressed. The browser revalidates with If-None-Match on every
// load (no-cache); an unchanged page is answered with 304 and no body.
static void sendPage(AsyncWebServerRequest* r, const uint8_t* gz, size_t len, const char* etag) {
  const AsyncWebHeader* inm = r->getHeader("If-None-Match");
  if (inm && inm->value() == etag) {
    AsyncWebServerResponse* res = r->beginResponse(304);
    res->addHeader("ETag", etag);
    res->addHeader("Cache-Control", "no-cache");
    r->send(res);
    return;
  }
  AsyncWebServerResponse* res = r->beginResponse(200, "text/html", gz, len);
  res->addHeader("Content-Encoding", "gzip");
  res->addHeader("ETag", etag);
  res->addHeader("Cache-Control", "no-cache");
  r->send(res);
}

// Profile display names for /api/profiles – reading every profile file on each call is slow,
// so they are cached until a profile is saved, deleted or restored (all handlers run in the web task)
static std::vector<std::pair<String, String>> profileNames;
static bool profileNamesValid = false;
static void invalidateProfileNames() { profileNamesValid = false; }

// Profile in use by an enabled car? (call under Poller::mutex())
static bool profileInUse(const String& name) {
  for (auto& c : cfg.cars) if (c.enabled && c.profile == name) return true;
  return false;
}
static int carParam(AsyncWebServerRequest* r, int dflt) {
  if (!r->hasParam("car")) return dflt;
  return constrain((int)r->getParam("car")->value().toInt(), 0, NCARS - 1);
}

void begin() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    sendPage(r, INDEX_HTML_GZ, sizeof(INDEX_HTML_GZ), INDEX_HTML_ETAG);
  });

  // User manual (stored in flash)
  server.on("/help", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    sendPage(r, HELP_HTML_GZ, sizeof(HELP_HTML_GZ), HELP_HTML_ETAG);
  });
  server.on("/hilfe", HTTP_GET, [](AsyncWebServerRequest* r) { r->redirect("/help"); });   // old bookmark

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    JsonDocument d;
    Poller::statusJson(d);
    d["reset_reason"] = Watchdog::resetReason();
    d["safe_mode"] = Watchdog::safeMode();
    sendJson(r, d);
  });

  // Last 60 log lines; the UI calls this only when log_seq in /api/status changed
  server.on("/api/logtail", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    JsonDocument d;
    d["seq"] = logSeq();
    JsonArray lg = d["lines"].to<JsonArray>();
    logToJson(lg);
    sendJson(r, d);
  });

  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    JsonDocument d;
    {
      std::lock_guard<std::mutex> lk(Poller::mutex());
      configToJson(d, true);
    }
    sendJson(r, d);
  });

  postJson("/api/config", [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    String b = body(r);
    JsonDocument d;
    if (b.isEmpty() || deserializeJson(d, b)) return sendMsg(r, false, "Invalid JSON");
    Poller::requestConfigApply(b);
    if (r->hasParam("reboot")) rebootAt = millis() + 1500;
    sendMsg(r, true, r->hasParam("reboot") ? "Saved – restarting…" : "Saved");
  });

  server.on("/api/profiles", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    JsonDocument d;
    {
      std::lock_guard<std::mutex> lk(Poller::mutex());
      JsonArray act = d["active"].to<JsonArray>();   // profile per car ("" = car disabled)
      for (auto& c : cfg.cars) act.add(c.enabled ? c.profile : String());
    }
    if (!profileNamesValid) {
      profileNames.clear();
      JsonDocument filter;
      filter["name"] = true;
      for (auto& n : listProfiles()) {
        JsonDocument p;   // only read the display name
        String nm;
        if (!deserializeJson(p, readProfileRaw(n), DeserializationOption::Filter(filter)) && p["name"].is<const char*>())
          nm = p["name"].as<String>();
        profileNames.emplace_back(n, nm);
      }
      profileNamesValid = true;
    }
    JsonArray a = d["list"].to<JsonArray>();
    JsonObject names = d["names"].to<JsonObject>();
    for (auto& pn : profileNames) {
      a.add(pn.first);
      if (pn.second.length()) names[pn.first] = pn.second;
    }
    sendJson(r, d);
  });

  server.on("/api/profile", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    String name = r->hasParam("name") ? r->getParam("name")->value() : "";
    String raw = readProfileRaw(name);
    if (raw.isEmpty()) return sendMsg(r, false, "Profile not found", 404);
    r->send(200, "application/json", raw);
  });

  postJson("/api/profile", [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    String name = r->hasParam("name") ? r->getParam("name")->value() : "";
    String err;
    if (!saveProfileRaw(name, body(r), err)) return sendMsg(r, false, err);
    invalidateProfileNames();
    bool active;
    {
      std::lock_guard<std::mutex> lk(Poller::mutex());
      active = profileInUse(name);
    }
    if (active) Poller::requestProfileReload();
    logf("Profile '%s' saved", name.c_str());
    sendMsg(r, true, active ? "Saved and activated" : "Saved");
  });

  server.on("/api/profile-delete", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    String name = r->hasParam("name") ? r->getParam("name")->value() : "";
    bool active;
    {
      std::lock_guard<std::mutex> lk(Poller::mutex());
      active = profileInUse(name);
    }
    if (active) return sendMsg(r, false, "This profile is used by a car and cannot be deleted");
    invalidateProfileNames();
    if (!validProfileName(name) || !LittleFS.remove("/profiles/" + name + ".json"))
      return sendMsg(r, false, "Delete failed");
    sendMsg(r, true, "Deleted");
  });

  server.on("/api/profile-defaults", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    installDefaultProfiles(true);
    invalidateProfileNames();
    Poller::requestProfileReload();
    sendMsg(r, true, "Factory profiles restored");
  });

  postJson("/api/job", [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    JsonDocument d;
    if (deserializeJson(d, body(r))) return sendMsg(r, false, "Invalid JSON");
    String t = d["type"] | "";
    Poller::JobType jt = t == "scan" ? Poller::JOB_SCAN : t == "test" ? Poller::JOB_TEST : t == "diag" ? Poller::JOB_DIAG : Poller::JOB_RAW;
    String cmd = d["cmd"] | "";
    if (jt != Poller::JOB_SCAN && jt != Poller::JOB_DIAG && cmd.isEmpty()) return sendMsg(r, false, "Command missing");
    if (Poller::isPaused()) return sendMsg(r, false, "Not available in safe mode / during an update", 409);
    if (!Poller::submitJob(jt, d["car"] | 0, d["header"] | "", cmd, d["formula"] | "", d["cap"] | 0.0f, d["cons"] | 0.0f))
      return sendMsg(r, false, "Another job is already running", 409);
    sendMsg(r, true, "Started");
  });

  server.on("/api/job", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    JsonDocument d;
    Poller::jobJson(d);
    sendJson(r, d);
  });

  // WiFi scan (async): 1st call starts the scan, subsequent calls return the result
  server.on("/api/wifiscan", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    JsonDocument d;
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED || r->hasParam("start")) {
      if (n != WIFI_SCAN_RUNNING) {
        WiFi.scanDelete();
        WiFi.scanNetworks(true, false);   // async, without hidden networks
      }
      d["state"] = "running";
    } else if (n == WIFI_SCAN_RUNNING) {
      d["state"] = "running";
    } else {
      d["state"] = "done";
      JsonArray arr = d["networks"].to<JsonArray>();
      for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.isEmpty()) continue;
        bool dup = false;   // same SSID (multiple APs/mesh) only once, strongest signal
        for (JsonObject o : arr)
          if (o["ssid"] == ssid) {
            dup = true;
            if (WiFi.RSSI(i) > o["rssi"].as<int>()) o["rssi"] = WiFi.RSSI(i);
          }
        if (dup) continue;
        JsonObject o = arr.add<JsonObject>();
        o["ssid"] = ssid;
        o["rssi"] = WiFi.RSSI(i);
        o["ch"] = WiFi.channel(i);
        o["open"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
      }
      WiFi.scanDelete();
    }
    sendJson(r, d);
  });

  // Download log: ?old=1 = previous (rotated) file, otherwise current log
  server.on("/api/log", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    bool old = r->hasParam("old");
    logFlush();
    // Send in chunks from flash, each read under the file lock, so the main loop cannot rotate/delete
    // the file mid-read (cause of crashes up to 0.3.5) – no large RAM buffer
    AsyncWebServerResponse* res;
    if (logPersist() && logFileExists(old)) {
      res = r->beginChunkedResponse("text/plain; charset=utf-8", [old](uint8_t* buf, size_t maxLen, size_t index) -> size_t {
        return logReadChunk(old, index, buf, maxLen);
      });
    } else if (old) {
      return sendMsg(r, false, "No older log available", 404);
    } else {
      res = r->beginResponse(200, "text/plain; charset=utf-8", logText());
    }
    if (r->hasParam("dl"))
      res->addHeader("Content-Disposition", String("attachment; filename=\"obd2mqtt-") +
                                              (old ? "log-alt" : "log") + ".txt\"");
    r->send(res);
  });

  server.on("/api/log-clear", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    logClear();
    logf("%s", "Log cleared");
    sendMsg(r, true, "Log cleared");
  });

  // ---------- Backup & restore ----------
  // GET /api/backup?secrets=1  → JSON file with settings + all profiles.
  // Sent in chunks: header (settings) and then each profile directly from its file.
  // Previously everything was assembled in RAM (JSON document + text + copy in the response, ~3 × 30 KB) –
  // with low heap this failed and the file was 0 bytes.
  server.on("/api/backup", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    bool secrets = r->hasParam("secrets") && r->getParam("secrets")->value() == "1";
    struct Bk { String piece; size_t pos = 0; std::vector<String> names; size_t next = 0; bool done = false, first = true; };
    auto st = std::make_shared<Bk>();
    char ts[24] = "";
    {
      JsonDocument d;
      d["type"] = "obd2mqtt-backup";
      d["format"] = 1;
      d["fw"] = FW_VERSION;
      if (timeValid()) {
        time_t now = time(nullptr);
        struct tm t;
        localtime_r(&now, &t);
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &t);
        d["created"] = ts;
      }
      d["with_secrets"] = secrets;
      JsonObject c = d["config"].to<JsonObject>();
      {
        JsonDocument cd;
        {
          std::lock_guard<std::mutex> lk(Poller::mutex());
          configToJson(cd, !secrets);
        }
        c.set(cd.as<JsonObjectConst>());
      }
      serializeJsonPretty(d, st->piece);
    }
    // Header without closing brace, followed by "profiles"
    int k = st->piece.lastIndexOf('}');
    if (k < 0 || st->piece.isEmpty()) return sendMsg(r, false, "Not enough memory for the backup", 500);
    st->piece = st->piece.substring(0, k);
    while (st->piece.length() && isspace((unsigned char)st->piece[st->piece.length() - 1])) st->piece.remove(st->piece.length() - 1);
    st->piece += ",\n  \"profiles\": {";
    for (auto& n : listProfiles()) st->names.push_back(n);

    AsyncWebServerResponse* res = r->beginChunkedResponse("application/json", [st](uint8_t* buf, size_t maxLen, size_t) -> size_t {
      size_t out = 0;
      while (out < maxLen) {
        if (st->pos >= st->piece.length()) {
          if (st->done) break;
          // produce the next chunk: one profile (unchanged from its file) or the closing part
          st->piece = "";
          st->pos = 0;
          while (st->piece.isEmpty() && st->next < st->names.size()) {
            const String& n = st->names[st->next];
            String raw = readProfileRaw(n);
            raw.trim();
            st->next++;
            if (!raw.startsWith("{") || !raw.endsWith("}")) continue;   // skip broken file
            st->piece = String(st->first ? "\n" : ",\n") + "    \"" + n + "\": " + raw;
            st->first = false;
          }
          if (st->piece.isEmpty()) { st->piece = "\n  }\n}\n"; st->done = true; }
        }
        size_t n = min(maxLen - out, st->piece.length() - st->pos);
        memcpy(buf + out, st->piece.c_str() + st->pos, n);
        st->pos += n;
        out += n;
      }
      return out;
    });
    String date = ts[0] ? String(ts).substring(0, 10) : String("backup");
    res->addHeader("Content-Disposition", "attachment; filename=\"obd2mqtt-backup-" + date + ".json\"");
    res->addHeader("Cache-Control", "no-store");
    r->send(res);
  });

  // POST /api/restore?config=1&profiles=1  (body = backup JSON) → restart afterwards
  postJson("/api/restore", [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    bool doCfg = !r->hasParam("config") || r->getParam("config")->value() == "1";
    bool doProf = !r->hasParam("profiles") || r->getParam("profiles")->value() == "1";
    JsonDocument d;
    char* raw = (char*)r->_tempObject;
    if (!raw || !raw[0]) return sendMsg(r, false, "File empty, too large (max. 64 KB) or not enough memory");
    // parse directly in the received buffer (no String copy – saves a lot of RAM with a 30 KB backup)
    if (auto e = deserializeJson(d, raw)) return sendMsg(r, false, String("Not a valid JSON file: ") + e.c_str());
    if (d["type"] != "obd2mqtt-backup") return sendMsg(r, false, "This is not an OBD2MQTT backup");
    int okP = 0;
    String errs;
    if (doProf) {
      for (JsonPair kv : d["profiles"].as<JsonObject>()) {
        String js, err;
        serializeJson(kv.value(), js);
        if (saveProfileRaw(kv.key().c_str(), js, err)) okP++;
        else errs += String(kv.key().c_str()) + ": " + err + "; ";
      }
      invalidateProfileNames();
    }
    bool cfgDone = false;
    if (doCfg && d["config"].is<JsonObject>()) {
      String js;
      serializeJson(d["config"], js);
      Poller::requestConfigApply(js);
      cfgDone = true;
    }
    if (!cfgDone && okP == 0) return sendMsg(r, false, String("Nothing restored. ") + errs);
    String msg = "Restored: ";
    if (cfgDone) msg += String("settings") + (d["with_secrets"] ? "" : " (passwords unchanged)");
    if (okP) msg += String(cfgDone ? " + " : "") + okP + " profile(s)";
    if (errs.length()) msg += String(". Errors: ") + errs;
    msg += " – restarting…";
    logf("Backup restored (%s)", d["created"] | "-");
    rebootAt = millis() + 2500;   // WiFi/MQTT settings only take effect after a restart
    sendMsg(r, true, msg);
  });

  server.on("/api/pollnow", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    Poller::requestPollNow(carParam(r, -1));   // no car = all cars
    sendMsg(r, true, "Poll triggered");
  });

  server.on("/api/checknow", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    Poller::requestCheckNow(carParam(r, 0));
    sendMsg(r, true, "Check started");
  });

  server.on("/api/ledtest", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    int pin = r->hasParam("pin") ? r->getParam("pin")->value().toInt() : cfg.ledPin;
    if (!Led::pinOk(pin)) { sendMsg(r, false, "GPIO not usable for the LED"); return; }
    Led::test(pin);
    sendMsg(r, true, "LED test: red, green, blue");
  });

  server.on("/api/diag", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    AsyncWebServerResponse* res = r->beginChunkedResponse("text/plain; charset=utf-8", [](uint8_t* buf, size_t maxLen, size_t index) -> size_t {
      return Poller::diagChunk(index, buf, maxLen);
    });
    if (r->hasParam("dl")) res->addHeader("Content-Disposition", "attachment; filename=\"obd2mqtt-diagnose.txt\"");
    r->send(res);
  });

  server.on("/api/debug", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    uint32_t m = r->hasParam("min") ? r->getParam("min")->value().toInt() : 0;
    if (m > 720) m = 720;
    Poller::setDebug(m);
    sendMsg(r, true, m ? "Diagnostic log switched on" : "Diagnostic log switched off");
  });

  server.on("/api/polling", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    bool on = r->hasParam("on") && r->getParam("on")->value() == "1";
    Poller::requestEnabled(carParam(r, 0), on);
    sendMsg(r, true, on ? "Polling switched on" : "Polling switched off");
  });

  server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    Watchdog::clearCrashCounter();   // manual restart → safe mode ends
    rebootAt = millis() + 1000;
    sendMsg(r, true, "Restarting…");
  });

  // OTA firmware update (firmware.bin)
  // Online update from GitHub: GET = status, POST ?action=check|install
  server.on("/api/online-update", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    JsonDocument d;
    OnlineUpdate::stateJson(d);
    sendJson(r, d);
  });
  server.on("/api/online-update", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    String a = r->hasParam("action") ? r->getParam("action")->value() : "";
    if (a == "check") { OnlineUpdate::requestCheck(); return sendMsg(r, true, "Check started"); }
    if (a == "install") {
      if (OnlineUpdate::requestInstall()) return sendMsg(r, true, "Update started");
      return sendMsg(r, false, "No newer version known – check first");
    }
    sendMsg(r, false, "action?");
  });

  // OTA firmware update (obd2mqtt-vX.Y.Z-esp32-ota.bin)
  server.on("/api/update", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!auth(r)) return;
    JsonDocument d;
    static const char* ST[] = {"idle", "running", "done", "error"};
    d["state"] = ST[otaState];
    d["written"] = (uint32_t)otaWritten;
    d["total"] = (uint32_t)otaTotal;
    d["error"] = otaError;
    sendJson(r, d);
  });

  server.on("/api/update", HTTP_POST,
    [](AsyncWebServerRequest* r) {
      if (!auth(r)) return;
      bool ok = otaState == 2;
      if (!ok && otaState != 3) { otaState = 3; if (otaError.isEmpty()) otaError = "No data received"; }
      sendMsg(r, ok, ok ? String("Update successful – restarting…") : String("Update failed: ") + otaError);
      if (ok || Poller::bleStopped()) rebootAt = millis() + 1500;   // Bluetooth was switched off → restart brings it back
      else Poller::setPaused(false);
    },
    [](AsyncWebServerRequest* r, const String& fn, size_t index, uint8_t* data, size_t len, bool final) {
      if (!cfg.webPass.isEmpty() && !r->authenticate("admin", cfg.webPass.c_str())) return;
      if (index == 0) {
        otaError = "";
        otaWritten = 0;
        otaTotal = r->contentLength();
        otaState = 1;
        Poller::setPaused(true, true);   // no BLE while flashing – Bluetooth stack off to lower the current
        logf("OTA update started: %s (%u bytes)", fn.c_str(), (unsigned)otaTotal);
        if (Update.isRunning()) Update.abort();       // leftovers from an aborted attempt
        uint16_t chip = len >= 14 ? (uint16_t)(data[12] | (data[13] << 8)) : 0xFFFF;
        if (len && data[0] != 0xE9) {
          otaError = fn.indexOf("factory") >= 0
                       ? "This is the factory.bin – please use the *-ota.bin for OTA"
                       : "Not a valid ESP32 firmware (wrong magic byte)";
          otaState = 3;
        } else if (len >= 14 && chip != CONFIG_IDF_FIRMWARE_CHIP_ID) {
          // e.g. a classic-ESP32 file on an ESP32-S3: the board would not start it
          otaError = String("Firmware for another chip (file: ") + chip + ", this board: " + CONFIG_IDF_FIRMWARE_CHIP_ID +
                     ") – use the *-" FW_BOARD "-ota.bin";
          otaState = 3;
        } else if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
          otaError = Update.errorString();
          otaState = 3;
        }
      }
      if (otaState != 1) return;
      if (len && Update.write(data, len) != len) {
        otaError = Update.errorString();
        otaState = 3;
        Update.abort();
        logf("OTA error: %s", otaError.c_str());
        return;
      }
      otaWritten = index + len;
      if (final) {
        if (Update.end(true)) {
          otaState = 2;
          logf("OTA update finished (%u bytes) – restarting", (unsigned)otaWritten);
        } else {
          otaError = Update.errorString();
          otaState = 3;
          logf("OTA error: %s", otaError.c_str());
        }
      }
    });

  server.onNotFound([](AsyncWebServerRequest* r) {
    // Captive portal: redirect unknown paths to the start page
    r->redirect("/");
  });

  server.begin();
}

void loop() {
  if (rebootAt && (int32_t)(millis() - rebootAt) >= 0) {
    logf("%s", "Restart via web interface");
    logFlush();
    delay(100);
    ESP.restart();
  }
}

}  // namespace Web
