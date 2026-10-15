#include "watchdog.h"
#include <map>
#include <LittleFS.h>
#include "poller.h"
#include <WiFi.h>
#include <vector>
#include "config.h"
#include "elm_ble.h"
#include "log.h"
#include "mqtt_ha.h"
#include "obd_parse.h"

namespace Poller {

static std::mutex mtx;
std::mutex& mutex() { return mtx; }

struct PidState {
  double value = NAN;
  uint32_t lastOk = 0;      // millis() of the last valid value
  uint32_t lastPoll = 0;    // millis() of the last attempt (0 = never)
  String err, raw;
};
static std::vector<PidState> states;

static String currentHeader;
static uint32_t nextConnectTry = 0;
static bool elmReady = false;         // init sequence done
static float lastVoltage = NAN;
static bool carAsleep = false;          // 12V below sleepVoltage → car asleep, no CAN traffic
static uint32_t lastSleepPoll = 0;
static bool forcePoll = false;          // "Poll now" bypasses sleep mode once
static ObdParse::ErrKind lastQueryKind = ObdParse::ERR_NONE;   // error kind of the last query()
static String lastError;
static uint32_t lastConnectOk = 0;
static bool needWarmup = false;          // after connect/init: send 3E00 once before the first long query
static bool unreachLogged = false;       // log "dongle sleeps with the car" only once
static uint32_t pollCycles = 0;
static String elmVersion, cacheName, cacheKind, cacheUuids;
static bool cacheBle = false, cacheMqtt = false;   // for the web task (no BLE calls there)
static int cacheRssi = 0;

// Flags from the web task
static volatile bool flagProfileReload = false;
static volatile bool flagPollNow = false;
static String pendingConfig;          // under mtx

// Job
struct Job {
  JobType type;
  String header, cmd, formula;
  float cap = 0, cons = 0;
  String state = "idle";
  String resultJson;
  String progress;
};
static Job job;
static uint32_t lastJobMs = 0;      // keep connection for another 60 s after test/terminal

// ---------------------------------------------------------------

static void setError(const String& e) {
  logf("Error: %s", e.c_str());
  {
    std::lock_guard<std::mutex> lk(mtx);
    lastError = e;
  }
  MqttHa::publishText("last_error", e);
}

static void resetStates() {
  std::lock_guard<std::mutex> lk(mtx);
  states.assign(profile.pids.size(), PidState());
}

static void reloadProfile() {
  Profile old = profile;
  bool ok;
  {
    std::lock_guard<std::mutex> lk(mtx);
    ok = loadProfile(cfg.profile);
    if (!ok) profile = old;
  }
  if (!ok) setError(String("Profile '") + cfg.profile + "' could not be loaded");
  else logf("Profile '%s' loaded (%u PIDs)", cfg.profile.c_str(), (unsigned)profile.pids.size());
  resetStates();
  elmReady = false;           // init commands may have changed
  MqttHa::requestDiscovery();
}

static bool elmCmd(const String& c, String& resp, uint32_t timeout = 0) {
  bool ok = ElmBle::command(c, resp, timeout ? timeout : cfg.cmdTimeoutMs);
  resp.trim();
  return ok;
}

// ---------- Diagnostics ----------
static volatile uint32_t debugUntil = 0;
static bool debugOn() { return debugUntil && (int32_t)(millis() - debugUntil) < 0; }
void setDebug(uint32_t minutes) {
  debugUntil = minutes ? millis() + minutes * 60000UL : 0;
  if (minutes) logf("Diagnostic log on (%lu min)", (unsigned long)minutes);
  else logf("%s", "Diagnostic log off");
}
static String oneLine(const String& s, size_t maxLen = 300) {
  String r = s;
  r.replace("\r", "|");
  r.replace("\n", "");
  while (r.endsWith("|")) r.remove(r.length() - 1);
  if (r.length() > maxLen) r = r.substring(0, maxLen) + "…";
  return r;
}
static bool rejected(const String& r) { return r.indexOf('?') >= 0 || r.indexOf("ERROR") >= 0; }

static bool ensureReady(String& err) {
  if (!ElmBle::connected()) {
    elmReady = false;
    if (!ElmBle::connect(err)) return false;
    std::lock_guard<std::mutex> lk(mtx);
    lastConnectOk = millis();
  }
  if (!elmReady) {
    String r;
    // In sleep mode we connect every minute only to read the voltage → no [diag] lines then
    bool dbg = debugOn() && !ElmBle::quiet;
    Watchdog::step("ELM init");
    for (auto& c : profile.init) {
      bool ok = elmCmd(c, r, c.startsWith("ATZ") || c.startsWith("AT Z") ? 4000 : 0);
      if (!ElmBle::connected()) {   // connection lost → abort init immediately instead of "no prompt" on every line
        err = String("Connection dropped during initialisation (at ") + c + ")";
        return false;
      }
      if (!ok) logf("Init '%s' without prompt (response: %s)", c.c_str(), oneLine(r).c_str());
      else if (rejected(r)) logf("Init '%s' rejected by the dongle (response: %s)", c.c_str(), oneLine(r).c_str());
      else if (dbg) logf("[diag] %s → %s", c.c_str(), oneLine(r, 60).c_str());
      if (c.startsWith("ATZ")) delay(300);
    }
    currentHeader = "";
    // Warm-up: many ELM clones return "CAN ERROR" or truncated responses
    // on the first CAN message right after reset.
    delay(300);
    elmCmd("ATRV", r);
    if (dbg) {
      String dp;
      elmCmd("ATDPN", dp);
      logf("[diag] ATRV → %s, ATDPN → %s, MTU %u", oneLine(r, 20).c_str(), oneLine(dp, 20).c_str(), (unsigned)ElmBle::mtu());
    }
    if (elmCmd("ATI", r) && r.length() && r.length() < 40) {   // e.g. "ELM327 v1.5"
      r.replace("\r", " "); r.trim();
      std::lock_guard<std::mutex> lk(mtx);
      elmVersion = r;
    }
    if (!ElmBle::connected()) { err = "Connection dropped during initialisation"; return false; }
    elmReady = true;
    needWarmup = true;
    Watchdog::step("Poller");
    MqttHa::publishText("car", "online");
    MqttHa::publishValue("ble_rssi", ElmBle::rssi(), 0);
  }
  return true;
}

// Custom flow control (ATFCSM1) in the profile? Then the flow control address must follow
// every address change (e.g. IONIQ: 7E4 for the BMS, 7C6 for the odometer).
static bool userFlowControl() {
  for (auto& c : profile.init) {
    String u = c;
    u.toUpperCase();
    u.replace(" ", "");
    if (u == "ATFCSM1") return true;
  }
  return false;
}

static bool setHeader(const String& h) {
  if (h.isEmpty() || h == currentHeader) return true;
  String r;
  if (!elmCmd("ATSH" + h, r) || r.indexOf("OK") < 0) {
    currentHeader = "";
    return false;
  }
  if (userFlowControl() && (!elmCmd("ATFCSH" + h, r) || r.indexOf("OK") < 0)) {
    currentHeader = "";
    return false;
  }
  currentHeader = h;
  return true;
}

static float readVoltage() {
  String r;
  if (!elmCmd("ATRV", r)) return NAN;
  float v = r.toFloat();   // "12.4V"
  return (v > 5 && v < 20) ? v : NAN;
}

// Runs header+cmd and returns bytes
static bool query(const String& header, const String& cmd, std::vector<uint8_t>& bytes,
                  String& raw, String& err) {
  if (!setHeader(header)) {
    err = ElmBle::connected() ? "ATSH" + header + " failed" : String("BLE disconnected");
    return false;
  }
  for (int attempt = 0; attempt < 3; attempt++) {   // retry up to 2× on incomplete response
    if (attempt) {
      String r = raw.substring(0, 80);
      r.replace("\r", "|");
      logf("%s: %s – retrying (raw: %s)", cmd.c_str(), err.c_str(), r.c_str());
      delay(500);
    }
    lastQueryKind = ObdParse::ERR_OTHER;
    if (!elmCmd(cmd, raw)) {
      err = ElmBle::connected() ? "Timeout (no prompt)" : "BLE disconnected";
      if (!ElmBle::connected()) return false;
      continue;
    }
    std::string e;
    bool ok = ObdParse::parseResponse(raw.c_str(), cmd.c_str(), bytes, e);
    err = e.c_str();
    lastQueryKind = ok ? ObdParse::ERR_NONE : ObdParse::lastErrKind;
    if (debugOn()) logf("[diag] %s %s → %s%s", header.c_str(), cmd.c_str(), oneLine(raw, 400).c_str(), ok ? "" : (String(" [") + err + "]").c_str());
    if (ok) return true;
    // Retry only on incomplete response or CAN error (typical shortly after connecting),
    // not on NO DATA / negative response
    bool retry = ObdParse::lastErrKind == ObdParse::ERR_INCOMPLETE ||
                 (ObdParse::lastErrKind == ObdParse::ERR_CAN && attempt == 0);
    if (!retry) return false;
  }
  return false;
}

// Pass the last values of all PIDs to formula evaluation (formulas may use other PIDs by ID)
static void syncValues() {
  std::vector<std::pair<std::string, double>> v;
  std::lock_guard<std::mutex> lk(mtx);
  for (size_t i = 0; i < profile.pids.size() && i < states.size(); i++)
    v.push_back({profile.pids[i].id.c_str(), profile.pids[i].enabled ? states[i].value : NAN});
  ObdParse::setValues(v);
}

static bool isDue(size_t i, uint32_t now) {
  const PidDef& p = profile.pids[i];
  if (!p.enabled) return false;
  const PidState& s = states[i];
  return s.lastPoll == 0 || now - s.lastPoll >= p.interval * 1000UL;
}

static uint32_t msUntilNextDue(uint32_t now) {
  uint32_t best = UINT32_MAX;
  for (size_t i = 0; i < profile.pids.size(); i++) {
    const PidDef& p = profile.pids[i];
    if (!p.enabled) continue;
    const PidState& s = states[i];
    if (s.lastPoll == 0) return 0;
    uint32_t el = now - s.lastPoll, iv = p.interval * 1000UL;
    best = min(best, el >= iv ? 0 : iv - el);
  }
  return best;
}

// All current values incl. profile as one JSON: <base>/state
static void publishStateJson() {
  JsonDocument d;
  d["profile"] = profile.name.length() ? profile.name : cfg.profile;
  d["profile_id"] = cfg.profile;
  if (timeValid()) {
    time_t tnow = time(nullptr);
    struct tm tmv;
    localtime_r(&tnow, &tmv);
    char tb[32];
    strftime(tb, sizeof(tb), "%Y-%m-%dT%H:%M:%S%z", &tmv);
    d["time"] = tb;
  }
  JsonObject v = d["values"].to<JsonObject>();
  {
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < profile.pids.size() && i < states.size(); i++) {
      if (!profile.pids[i].enabled || isnan(states[i].value)) continue;
      v[profile.pids[i].id] = serialized(String(states[i].value, profile.pids[i].precision));
    }
    if (!isnan(lastVoltage)) d["voltage_12v"] = serialized(String(lastVoltage, 1));
  }
  String s;
  serializeJson(d, s);
  MqttHa::publishText("state", s);
}

static void pollDue() {
  uint32_t now = millis();
  String err;
  const uint32_t drops0 = ElmBle::dropCount();
  if (!ensureReady(err)) {
    MqttHa::publishText("car", "offline");
    if (!cfg.keepConnected || !ElmBle::connected()) { ElmBle::disconnect(); elmReady = false; }
    nextConnectTry = millis() + cfg.retrySec * 1000UL;
    // When the car sleeps, the dongle (e.g. WiCAN) often sleeps too → don't repeat the same message every minute
    if (carAsleep && cfg.sleepVoltage > 0) {
      if (!unreachLogged)
        logf("BLE: %s – dongle probably sleeps with the car, checking every minute (not reported again)", err.c_str());
      unreachLogged = true;
    } else
      logf("BLE: %s – retry in %lus", err.c_str(), (unsigned long)cfg.retrySec);
    std::lock_guard<std::mutex> lk(mtx);
    lastError = err;
    return;
  }
  if (unreachLogged) {
    unreachLogged = false;
    logf("%s", "Dongle reachable again");
  }

  // 12V protection
  float v = readVoltage();
  {
    std::lock_guard<std::mutex> lk(mtx);
    lastVoltage = v;
  }
  if (!isnan(v)) MqttHa::publishValue("voltage_12v", v, 1);
  if (cfg.minVoltage > 0 && !isnan(v) && v < cfg.minVoltage) {
    setError(String("12V too low (") + String(v, 1) + " V) – polling paused");
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < states.size(); i++) if (isDue(i, now)) states[i].lastPoll = now;
    ElmBle::disconnect();
    elmReady = false;
    nextConnectTry = now + max(cfg.backoffSec, (uint32_t)900) * 1000UL;
    return;
  }

  // Sleep mode: if the 12V voltage is below the threshold, the car is off and not charging (no DC/DC converter).
  // Any CAN request would wake it – so only watch the voltage and don't poll anything.
  bool force = forcePoll;
  forcePoll = false;
  // Voltage unreadable: keep the last state (better not to poll than to wake the car)
  if (cfg.sleepVoltage > 0 && isnan(v) && carAsleep && !force) {
    if (!cfg.keepConnected) { ElmBle::disconnect(); elmReady = false; }
    nextConnectTry = now + 60000UL;
    return;
  }
  // 12V warning (with 0.2 V hysteresis), independent of sleep mode
  if (cfg.lowBattVoltage > 0 && !isnan(v)) {
    static int lowState = -1;
    int ls = lowState;
    if (v < cfg.lowBattVoltage) ls = 1;
    else if (v >= cfg.lowBattVoltage + 0.2f) ls = 0;
    else if (ls < 0) ls = 0;
    if (ls != lowState) {
      lowState = ls;
      MqttHa::publishText("battery_low", ls ? "ON" : "OFF");
      if (ls) logf("WARNING: 12 V battery low (%.1f V < %.1f V)", v, cfg.lowBattVoltage);
    }
  }
  if (cfg.sleepVoltage > 0 && !isnan(v)) {
    // Hysteresis like WiCAN: sleep below the threshold, awake only from threshold + 0.1 V
    bool asleep = carAsleep ? (v < cfg.sleepVoltage + 0.1f) : (v < cfg.sleepVoltage);
    if (asleep != carAsleep) {
      carAsleep = asleep;
      ElmBle::quiet = asleep;   // connect every minute while asleep → don't log each time
      MqttHa::publishText("car_awake", asleep ? "OFF" : "ON");
      if (asleep) {
        logf("Car asleep (12V %.1f V < %.1f V) – no queries so it can sleep", v, cfg.sleepVoltage);
        // A sleeping car isn't charging: set charging values to 0, otherwise HA would keep the last value
        bool any = false;
        for (size_t i = 0; i < profile.pids.size() && i < states.size(); i++) {
          const PidDef& p = profile.pids[i];
          if (!p.enabled || !p.sleepZero) continue;
          {
            std::lock_guard<std::mutex> lk(mtx);
            states[i].value = 0;
          }
          MqttHa::publishValue(p.id, 0, p.precision);
          any = true;
        }
        if (any) publishStateJson();
      }
      else
        logf("Car awake (12V %.1f V) – normal polling", v);
    }
    if (asleep && !force) {
      // Wake periodically only if the 12 V battery can handle it (like WiCAN: above 11.9 V)
      bool sleepPollDue = cfg.sleepPollMin > 0 && v > 11.9f &&
                          (lastSleepPoll == 0 || now - lastSleepPoll >= cfg.sleepPollMin * 60000UL);
      if (!sleepPollDue) {
        if (!cfg.keepConnected) { ElmBle::disconnect(); elmReady = false; }
        nextConnectTry = now + 60000UL;   // check only the voltage again in 1 min (doesn't wake the car)
        return;
      }
      lastSleepPoll = now;
      logf("Polling while asleep (every %lu min)", (unsigned long)cfg.sleepPollMin);
    } else if (asleep && force) {
      logf("%s", "“Poll now” – car is asleep and will be woken up");
    }
  }

  // Warm-up (only if the car is awake or may be woken): the first long message after
  // connecting often arrives truncated (IONIQ gateway). A short "Tester Present" (3E00) to the
  // ECU of the first query wakes up the path beforehand – the response is ignored.
  if (needWarmup) {
    needWarmup = false;
    for (size_t i = 0; i < profile.pids.size(); i++) {
      if (!isDue(i, now) || profile.pids[i].header.isEmpty()) continue;
      Watchdog::step("warm-up 3E00");
      if (setHeader(profile.pids[i].header)) {
        String r;
        elmCmd("3E00", r, 1500);
        if (debugOn()) logf("[diag] %s 3E00 → %s", profile.pids[i].header.c_str(), oneLine(r, 60).c_str());
        delay(100);
      }
      Watchdog::step("Poller");
      break;
    }
  }

  // Poll due PIDs; send identical header+cmd only once
  // Cache responses per cycle: send identical header+cmd only once, even if the PIDs aren't adjacent
  struct QRes { bool ok; std::vector<uint8_t> bytes; String raw, err; };
  std::map<String, QRes> cache;
  std::vector<uint8_t> bytes;
  String raw, qerr;
  bool qok = false, anyOk = false, anyTried = false, anyReply = false, anyErr = false;
  for (size_t i = 0; i < profile.pids.size(); i++) {
    if (!isDue(i, now)) continue;
    const PidDef& p = profile.pids[i];
    String key = p.header + "|" + p.cmd;
    auto it = cache.find(key);
    if (it == cache.end()) {
      Watchdog::step(("query " + p.cmd).c_str());
      qok = query(p.header, p.cmd, bytes, raw, qerr);
      Watchdog::step("Poller");
      // car responded (even if incomplete/negative) → it's awake, no sleep pause
      if (qok || lastQueryKind == ObdParse::ERR_INCOMPLETE || lastQueryKind == ObdParse::ERR_NRC) anyReply = true;
      cache[key] = QRes{qok, bytes, raw, qerr};
    } else {
      qok = it->second.ok; bytes = it->second.bytes; raw = it->second.raw; qerr = it->second.err;
    }
    double val = NAN;
    String perr = qerr;
    bool ok = qok;
    if (ok) {
      std::string e;
      syncValues();
      ok = ObdParse::evalFormula(p.formula.c_str(), bytes, val, e);
      perr = e.c_str();
      if (ok && ((!isnan(p.minVal) && val < p.minVal) || (!isnan(p.maxVal) && val > p.maxVal))) {
        ok = false;
        perr = String("Value ") + String(val, 2) + " out of range – discarded";
      }
    }
    {
      std::lock_guard<std::mutex> lk(mtx);
      PidState& s = states[i];
      s.lastPoll = now;
      s.raw = raw.substring(0, 300);
      s.err = ok ? "" : perr;
      if (ok) { s.value = val; s.lastOk = now; }
    }
    anyTried = true;
    anyOk |= ok;
    if (ok) {
      MqttHa::publishValue(p.id, val, p.precision);
      logf("%s = %s %s", p.id.c_str(), String(val, p.precision).c_str(), p.unit.c_str());
    } else {
      setError(p.id + ": " + perr);
      anyErr = true;
    }
    if (!ElmBle::connected()) { elmReady = false; break; }
  }
  pollCycles++;
  // Cycle without errors → reset "last error" (otherwise an old error would stay forever)
  // Cycle without errors → reset "last error" and always publish
  // (MQTT is retained: otherwise the old error would remain in the broker after a reboot)
  if (anyTried && !anyErr) {
    {
      std::lock_guard<std::mutex> lk(mtx);
      lastError = "";
    }
    MqttHa::publishText("last_error", "none");
  }
  publishStateJson();
  // Connection lost during polling: not a sign of a sleeping car → retry after 60 s
  if (ElmBle::dropCount() != drops0) {
    elmReady = false;
    nextConnectTry = millis() + 60000UL;
    logf("%s", "BLE connection dropped during polling – retry in 60s");
    {
      // failed PIDs due again on the next attempt (unpolled ones still are anyway)
      std::lock_guard<std::mutex> lk(mtx);
      for (size_t i = 0; i < states.size(); i++) if (states[i].lastPoll == now && !states[i].err.isEmpty()) states[i].lastPoll = 0;
    }
    return;
  }
  // No response (car asleep) → wait longer so nothing is kept awake
  if (anyTried && !anyOk && !anyReply && cfg.backoffSec > 0) {
    nextConnectTry = millis() + cfg.backoffSec * 1000UL;
    logf("No valid response – next attempt in %lus", (unsigned long)cfg.backoffSec);
    if (!cfg.keepConnected) { ElmBle::disconnect(); elmReady = false; }
  }
}

// ---------------------------------------------------------------

static String diagText;   // last diagnostic report – fetched via /api/diag instead of in the job JSON (saves RAM)
size_t diagChunk(size_t offset, uint8_t* buf, size_t maxLen) {
  std::lock_guard<std::mutex> lk(mtx);
  if (offset >= diagText.length()) return 0;
  size_t n = min(min(maxLen, (size_t)1024), diagText.length() - offset);
  memcpy(buf, diagText.c_str() + offset, n);
  return n;
}

static void setProgress(const String& p) {
  std::lock_guard<std::mutex> lk(mtx);
  job.progress = p;
}

// Diagnostic report: fresh connection, every init response, every query with raw data and bytes.
// Deliberately contains no Wi-Fi/IP/password data so it can be posted publicly.
static String diagReport() {
  String o;
  o.reserve(8000);
  auto ln = [&](const String& s) { o += s; o += '\n'; };
  ln("=== OBD2MQTT " + String("diagnostic report") + " ===");
  ln("Firmware: " FW_VERSION);
  if (timeValid()) {
    time_t tn = time(nullptr); struct tm tmv; localtime_r(&tn, &tmv);
    char tb[32]; strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M:%S", &tmv);
    ln(String("Time: ") + tb);
  }
  ln(String("Profile: ") + cfg.profile + " – " + profile.name + " (" + profile.model + ")");
  String mac = cfg.bleMac.length() == 17 ? cfg.bleMac.substring(0, 8) + ":xx:xx:xx" : cfg.bleMac;
  ln("Dongle: " + ElmBle::deviceName() + " / " + ElmBle::dongleKind() + " / " + mac);
  ln(String("Last restart: ") + Watchdog::resetReason() +
     (strlen(Watchdog::hangStep()) ? String(" – hung at: ") + Watchdog::hangStep() : String("")));
  ln(String("BLE connection parameters: ") +
     (ElmBle::relaxedParams() ? "slow (after repeated 0x208)" : "fast (7.5–15 ms)") +
     ", " + "drops since boot: " + String(ElmBle::dropCount()));
  if (!isnan(lastVoltage)) ln(String("12V (last reading): ") + String(lastVoltage, 1) + " V");

  setProgress("BLE");
  Watchdog::step("diag report");
  ElmBle::disconnect();
  elmReady = false;
  currentHeader = "";
  String err;
  // up to 3 attempts – a single glitch shouldn't make the whole report worthless
  bool con = false;
  for (int a = 0; a < 3 && !con; a++) {
    Watchdog::feed();
    if (a) { ln(String("BLE attempt ") + a + " failed: " + err); delay(2000); }
    err = "";
    con = ElmBle::connect(err);
  }
  if (!con) { ln(String("BLE connection failed: ") + err); Watchdog::step("Poller"); return o; }
  ln("BLE: RSSI " + String(ElmBle::rssi()) + " dBm, MTU " + String(ElmBle::mtu()) + ", UUIDs " + ElmBle::detectedUuids());

  ln("");
  ln("--- Init ---");
  int initBad = 0;
  for (auto& c : profile.init) {
    Watchdog::feed();
    String r;
    bool ok = elmCmd(c, r, c.startsWith("ATZ") || c.startsWith("AT Z") ? 4000 : 0);
    if (c.startsWith("ATZ")) delay(300);
    String flag = !ok ? "   <-- NO PROMPT" : rejected(r) ? "   <-- REJECTED" : "";
    if (flag.length()) initBad++;
    ln(c + " → " + oneLine(r, 80) + flag);
    if (!ElmBle::connected()) { ln("!!! BLE connection dropped – report incomplete"); Watchdog::step("Poller"); return o; }
  }
  delay(300);
  for (const char* c : {"ATI", "AT@1", "ATRV", "ATDPN"}) {
    Watchdog::feed();
    String r;
    elmCmd(c, r);
    ln(String(c) + " → " + oneLine(r, 80));
  }
  elmReady = true;
  {
    std::lock_guard<std::mutex> lk(mtx);
    lastConnectOk = millis();
  }

  ln("");
  ln(String("--- ") + "Queries" + " ---");
  std::vector<String> keys;
  for (auto& p : profile.pids) {
    String k = p.header + "|" + p.cmd;
    bool dup = false;
    for (auto& x : keys) if (x == k) dup = true;
    if (!dup) keys.push_back(k);
  }
  int nOk = 0, nInc = 0, nFail = 0, idx = 0;
  for (auto& k : keys) {
    Watchdog::feed();
    idx++;
    if (!ElmBle::connected()) { ln(""); ln("!!! BLE connection dropped – remaining queries skipped"); break; }
    setProgress(String(idx) + "/" + String(keys.size()));
    String header = k.substring(0, k.indexOf('|')), cmd = k.substring(k.indexOf('|') + 1);
    ln("");
    ln("[" + header + " " + cmd + "]");
    if (!setHeader(header)) { ln("  ATSH failed"); nFail++; continue; }
    std::vector<uint8_t> bytes;
    bool ok = false;
    ObdParse::ErrKind kind = ObdParse::ERR_OTHER;
    for (int a = 0; a < 3 && !ok; a++) {
      Watchdog::feed();
      if (a) delay(500);
      String raw;
      uint32_t t0 = millis();
      bool prompt = elmCmd(cmd, raw);
      uint32_t dt = millis() - t0;
      std::string e;
      ok = prompt && ObdParse::parseResponse(raw.c_str(), cmd.c_str(), bytes, e);
      kind = prompt ? (ok ? ObdParse::ERR_NONE : ObdParse::lastErrKind) : ObdParse::ERR_OTHER;
      ln("  " + String("attempt ") + String(a + 1) + " (" + String(dt) + " ms): " + oneLine(raw, 600) +
         (ok ? "" : String("  [") + (prompt ? e.c_str() : "no prompt") + "]"));
      if (!ok && !(kind == ObdParse::ERR_INCOMPLETE || kind == ObdParse::ERR_CAN)) break;
    }
    if (ok) {
      nOk++;
      String bl = "  Bytes (" + String(bytes.size()) + "):";
      for (size_t i = 0; i < bytes.size(); i++) {
        if (i % 8 == 0) { ln(bl); bl = "   "; }
        char b[12]; snprintf(b, sizeof(b), " B%u=%02X", (unsigned)i, bytes[i]);
        bl += b;
      }
      ln(bl);
    } else if (kind == ObdParse::ERR_INCOMPLETE) nInc++;
    else nFail++;
    for (auto& p : profile.pids) {
      if (p.header + "|" + p.cmd != k) continue;
      String line = "  -> " + p.id + (p.enabled ? "" : " (off)") + ": " + p.formula + " = ";
      if (ok) {
        double v; std::string e;
        syncValues();
        if (ObdParse::evalFormula(p.formula.c_str(), bytes, v, e)) line += String(v, p.precision) + " " + p.unit;
        else line += String("[") + e.c_str() + "]";
      } else line += "–";
      ln(line);
    }
  }
  ln("");
  ln(String("--- ") + "Summary" + " ---");
  Watchdog::step("Poller");
  ln(String("BLE: ") + (ElmBle::connected() ? String("RSSI ") + ElmBle::rssi() + " dBm" : String("connection lost at the end")) +
     ", " + "drops since boot: " + String(ElmBle::dropCount()));
  ln(String("Init problems: ") + initBad);
  ln(String("Queries OK: ") + nOk + " / " + keys.size() + ", " + "incomplete: " + nInc +
     ", " + "no answer/error: " + nFail);
  return o;
}

static void runJob() {
  Job j;
  {
    std::lock_guard<std::mutex> lk(mtx);
    if (job.state != "pending") return;
    job.state = "running";
    j = job;
  }
  JsonDocument r;
  if (j.type == JOB_DIAG) {
    logf("%s", "Creating diagnostic report…");
    String rep = diagReport();
    r["len"] = rep.length();
    {
      std::lock_guard<std::mutex> lk(mtx);
      diagText = "";          // free the old report first
      diagText = std::move(rep);
    }
    r["ok"] = true;
    logf("%s", "Diagnostic report done");
  } else if (j.type == JOB_SCAN) {
    Watchdog::step("BLE scan");
    ElmBle::disconnect();
    elmReady = false;
    auto list = ElmBle::scan(6000);
    Watchdog::step("Poller");
    JsonArray arr = r["devices"].to<JsonArray>();
    for (auto& e : list) {
      JsonObject o = arr.add<JsonObject>();
      o["mac"] = e.mac; o["name"] = e.name; o["rssi"] = e.rssi;
      o["random"] = e.addrRandom; o["obd"] = e.likelyObd;
    }
    r["ok"] = true;
  } else {
    String err;
    if (!ensureReady(err)) {
      r["ok"] = false;
      r["error"] = err;
    } else if (j.type == JOB_RAW) {
      String resp;
      bool ok = elmCmd(j.cmd, resp, 6000);
      String up = j.cmd; up.toUpperCase();
      if (up.startsWith("AT")) currentHeader = "";   // header may have been changed manually
      r["ok"] = ok;
      r["raw"] = resp;
      if (!ok) r["error"] = "No prompt (timeout)";
    } else {  // JOB_TEST
      std::vector<uint8_t> bytes;
      String raw;
      bool ok = query(j.header, j.cmd, bytes, raw, err);
      r["raw"] = raw;
      if (ok) {
        r["bytes"] = ObdParse::toHex(bytes).c_str();
        r["count"] = bytes.size();
        if (j.formula.length()) {
          double v, c0, k0;
          std::string e;
          ObdParse::getBattery(c0, k0);
          if (j.cap > 0 || j.cons > 0) ObdParse::setBattery(j.cap > 0 ? j.cap : c0, j.cons > 0 ? j.cons : k0);
          syncValues();
          bool fo = ObdParse::evalFormula(j.formula.c_str(), bytes, v, e);
          ObdParse::setBattery(c0, k0);
          if (fo) r["value"] = v;
          else { ok = false; err = e.c_str(); }
        }
      }
      r["ok"] = ok;
      if (!ok) r["error"] = err;
    }
  }
  String out;
  serializeJson(r, out);
  lastJobMs = millis();
  std::lock_guard<std::mutex> lk(mtx);
  job.resultJson = out;
  job.state = "done";
}

bool submitJob(JobType t, const String& header, const String& cmd, const String& formula, float cap, float cons) {
  std::lock_guard<std::mutex> lk(mtx);
  if (job.state == "pending" || job.state == "running") return false;
  job.type = t;
  job.header = header; job.header.toUpperCase();
  job.cmd = cmd; job.cmd.trim();
  if (t != JOB_RAW) job.cmd.toUpperCase();
  job.formula = formula;
  job.cap = cap;
  job.cons = cons;
  job.state = "pending";
  job.resultJson = "";
  job.progress = "";
  return true;
}

void jobJson(JsonDocument& d) {
  std::lock_guard<std::mutex> lk(mtx);
  d["state"] = job.state;
  if (job.progress.length() && job.state == "running") d["progress"] = job.progress;
  if (job.resultJson.length()) {
    JsonDocument r;
    deserializeJson(r, job.resultJson);
    d["result"] = r;
  }
}

// ---------------------------------------------------------------

void requestConfigApply(const String& json) {
  std::lock_guard<std::mutex> lk(mtx);
  pendingConfig = json;
}
void requestProfileReload() { flagProfileReload = true; }
String lastErrorText() {
  std::lock_guard<std::mutex> lk(mtx);
  return lastError;
}

void requestPollNow() { flagPollNow = true; forcePoll = true; }

static volatile int pendingEnable = -1;
void requestEnabled(bool on) { pendingEnable = on ? 1 : 0; }

static void applyEnable() {
  int pe = pendingEnable;
  if (pe < 0) return;
  pendingEnable = -1;
  bool on = pe == 1;
  MqttHa::publishText("polling", on ? "ON" : "OFF");
  if (cfg.pollEnabled == on) return;
  cfg.pollEnabled = on;
  saveConfig();
  logf("%s", on ? "Polling switched on" : "Polling switched off – no connection to the car");
  if (on) nextConnectTry = 0;
}

static void applyPendingConfig() {
  String js;
  {
    std::lock_guard<std::mutex> lk(mtx);
    if (pendingConfig.isEmpty()) return;
    js = pendingConfig;
    pendingConfig = "";
  }
  AppConfig before = cfg;
  JsonDocument d;
  if (deserializeJson(d, js)) return;
  {
    std::lock_guard<std::mutex> lk(mtx);
    configFromJson(d.as<JsonVariantConst>());
  }
  saveConfig();
  logSetPersist(cfg.logPersist);
  logf("%s", "Configuration saved");
  if (before.bleMac != cfg.bleMac || before.bleService != cfg.bleService ||
      before.bleNotify != cfg.bleNotify || before.bleWrite != cfg.bleWrite ||
      before.bleAddrType != cfg.bleAddrType) {
    ElmBle::disconnect();
    elmReady = false;
    nextConnectTry = 0;
  }
  if (before.profile != cfg.profile) reloadProfile();
  if (before.deviceName != cfg.deviceName) MqttHa::requestDiscovery();
  // Wi-Fi/MQTT changes take effect via reboot from the web UI
}

static bool bleOn = true;
void begin(bool bleEnabled) {
  bleOn = bleEnabled;
  if (bleOn) ElmBle::begin();
  reloadProfile();
}

static void updateCache() {
  bool b = ElmBle::connected();
  int r = b ? ElmBle::rssi() : 0;
  bool m = MqttHa::connected();
  static uint32_t lastInfo = 0;
  bool info = millis() - lastInfo > 2000;   // copy texts only occasionally
  String nm, kd, uu;
  if (info) { lastInfo = millis(); nm = ElmBle::deviceName(); kd = ElmBle::dongleKind(); uu = ElmBle::detectedUuids(); }
  std::lock_guard<std::mutex> lk(mtx);
  cacheBle = b; cacheRssi = r; cacheMqtt = m;
  if (info) { cacheName = nm; cacheKind = kd; cacheUuids = uu; }
}

static volatile bool paused = false;
bool isPaused() { return paused || !bleOn; }
void setPaused(bool p) {
  paused = p;
  if (p) logf("%s", "Polling paused (firmware update)");
}

void loop() {
  applyEnable();
  if (!bleOn) { applyPendingConfig(); if (flagProfileReload) { flagProfileReload = false; reloadProfile(); } updateCache(); return; }
  if (paused) { if (ElmBle::connected()) { ElmBle::disconnect(); elmReady = false; } updateCache(); return; }
  applyPendingConfig();
  if (flagProfileReload) { flagProfileReload = false; reloadProfile(); }
  runJob();

  // Main switch off: no automatic polling, disconnect dongle (test/terminal/"Poll now" keep working)
  if (!cfg.pollEnabled && !flagPollNow) {
    if (ElmBle::connected() && !cfg.keepConnected) {
      bool jobBusy;
      {
        std::lock_guard<std::mutex> lk(mtx);
        jobBusy = job.state == "pending" || job.state == "running";
      }
      if (!jobBusy && (!lastJobMs || millis() - lastJobMs > 60000)) { ElmBle::disconnect(); elmReady = false; }
    }
    updateCache();
    return;
  }

  uint32_t now = millis();
  if (flagPollNow) {
    flagPollNow = false;
    std::lock_guard<std::mutex> lk(mtx);
    for (auto& s : states) s.lastPoll = 0;
    nextConnectTry = 0;
  }
  if (cfg.bleMac.isEmpty()) { updateCache(); return; }

  uint32_t wait = msUntilNextDue(now);
  if (wait == 0) {
    if ((int32_t)(now - nextConnectTry) >= 0) pollDue();
  }
  if (ElmBle::connected() && !cfg.keepConnected && msUntilNextDue(millis()) > 20000) {
    bool jobBusy;
    {
      std::lock_guard<std::mutex> lk(mtx);
      jobBusy = job.state == "pending" || job.state == "running";
    }
    if (!jobBusy && (!lastJobMs || millis() - lastJobMs > 60000)) {
      ElmBle::disconnect();   // dongle may sleep
      elmReady = false;
    }
  }
  updateCache();
}

void statusJson(JsonDocument& d) {
  std::lock_guard<std::mutex> lk(mtx);
  uint32_t now = millis();
  d["fw"] = FW_VERSION;
  d["uptime"] = now / 1000;
  d["heap"] = ESP.getFreeHeap();
  d["heap_min"] = ESP.getMinFreeHeap();
  d["heap_block"] = ESP.getMaxAllocHeap();
  d["fs_used"] = LittleFS.usedBytes();
  d["fs_total"] = LittleFS.totalBytes();
  d["wifi"]["connected"] = WiFi.status() == WL_CONNECTED;
  d["wifi"]["ssid"] = WiFi.SSID();
  d["wifi"]["ip"] = WiFi.localIP().toString();
  d["wifi"]["rssi"] = WiFi.RSSI();
  d["wifi"]["gw"] = WiFi.gatewayIP().toString();
  d["wifi"]["mask"] = WiFi.subnetMask().toString();
  d["wifi"]["dns"] = WiFi.dnsIP(0).toString();
  d["wifi"]["ip_mode"] = g_ipFallback ? "fallback" : cfg.ipMode;
  d["mqtt"] = cacheMqtt;
  d["ble"]["connected"] = cacheBle;
  d["ble"]["mac"] = cfg.bleMac;
  d["ble"]["rssi"] = cacheRssi;
  d["ble"]["uuids"] = cacheUuids;
  d["ble"]["name"] = cacheName;
  d["ble"]["kind"] = cacheKind;
  d["ble"]["elm"] = elmVersion;
  d["ble"]["slow_params"] = ElmBle::relaxedParams();
  d["ble"]["drops"] = ElmBle::dropCount();
  d["hang_step"] = Watchdog::hangStep();
  if (timeValid()) {
    time_t tnow = time(nullptr);
    struct tm tmv;
    localtime_r(&tnow, &tmv);
    char tb[32];
    strftime(tb, sizeof(tb), "%d.%m.%Y %H:%M:%S %Z", &tmv);
    d["time"] = tb;
  }
  d["ble"]["last_ok_s"] = lastConnectOk ? (int32_t)((now - lastConnectOk) / 1000) : -1;
  d["ble"]["next_try_s"] = (int32_t)(nextConnectTry - now) > 0 ? (int32_t)((nextConnectTry - now) / 1000) : 0;
  if (!isnan(lastVoltage)) d["voltage"] = lastVoltage;
  d["car_asleep"] = cfg.sleepVoltage > 0 && carAsleep;
  d["poll_enabled"] = cfg.pollEnabled;
  d["debug_min"] = debugOn() ? (uint32_t)((debugUntil - millis()) / 60000UL) + 1 : 0;
  d["profile"] = cfg.profile;
  d["profile_name"] = profile.name;
  d["last_error"] = lastError;
  d["cycles"] = pollCycles;
  JsonArray arr = d["pids"].to<JsonArray>();
  for (size_t i = 0; i < profile.pids.size() && i < states.size(); i++) {
    const PidDef& p = profile.pids[i];
    const PidState& s = states[i];
    JsonObject o = arr.add<JsonObject>();
    o["id"] = p.id; o["name"] = p.name; o["unit"] = p.unit; o["enabled"] = p.enabled;
    if (!isnan(s.value)) o["value"] = serialized(String(s.value, p.precision));
    o["age_s"] = s.lastOk ? (int32_t)((now - s.lastOk) / 1000) : -1;
    if (s.lastPoll) {
      uint32_t el = now - s.lastPoll, iv = p.interval * 1000UL;
      o["next_s"] = el >= iv ? 0 : (iv - el) / 1000;
    } else o["next_s"] = 0;
    o["error"] = s.err;
  }
  JsonArray lg = d["log"].to<JsonArray>();
  logToJson(lg);
}

}  // namespace Poller
