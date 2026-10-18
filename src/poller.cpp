#include "watchdog.h"
#include <map>
#include <stdarg.h>
#include <LittleFS.h>
#include "poller.h"
#include <WiFi.h>
#include <vector>
#include "config.h"
#include "elm_ble.h"
#include "led.h"
#include "log.h"
#include "mqtt_ha.h"
#include "obd_parse.h"

// Two cars, one BLE connection at a time: each car has its own state (values, timers, sleep/backoff);
// the loop serves whichever car is due (connect → poll → disconnect → next car).
namespace Poller {

static std::mutex mtx;
std::mutex& mutex() { return mtx; }

struct PidState {
  double value = NAN;
  uint32_t lastOk = 0;      // millis() of the last valid value
  uint32_t lastPoll = 0;    // millis() of the last attempt (0 = never)
  String err, raw;
};

struct Car {
  int idx = 0;
  std::vector<PidState> states;
  String currentHeader;
  uint32_t nextConnectTry = 0;
  bool elmReady = false;              // init sequence done on the current connection
  float lastVoltage = NAN;
  bool carAsleep = false;             // 12V below sleepVoltage → car asleep, no CAN traffic
  uint32_t lastSleepPoll = 0;
  bool forcePoll = false;             // "Poll now" pending (only for the log – it never wakes a sleeping car)
  uint8_t awakePolls = 0;             // polls with data while awake and not charging (fast → slow interval)
  bool charging = false;              // last poll: PID "charging" or "charging_dc" = 1 → charging interval
  String lastError;
  uint32_t lastConnectOk = 0;
  uint32_t lastDataMs = 0;            // millis() of the last cycle with at least one valid value
  bool needWarmup = false;            // after connect/init: send 3E00 once before the first long query
  uint8_t quickRetries = 0;           // retries after 60 s when the dongle was in range but setup failed
  bool unreachLogged = false;         // log "dongle sleeps with the car" only once
  uint32_t pollCycles = 0;
  int lowState = -1;                  // 12V warning state (-1 = unknown)
  String elmVersion, cacheName, cacheKind, cacheUuids, cacheAddr;
  bool cacheBle = false;              // for the web task (no BLE calls there)
  int cacheRssi = 0;
  volatile bool flagReload = false;
  volatile bool flagPollNow = false;
  volatile bool flagCheckNow = false;
  volatile int pendingEnable = -1;
};
static Car cars[NCARS];
static Car* C = &cars[0];             // car currently being served
static CarConfig& ccfg() { return cfg.cars[C->idx]; }
static Profile& prof() { return profiles[C->idx]; }
static bool linkUp() { return ElmBle::connectedTo(ccfg()); }

static ObdParse::ErrKind lastQueryKind = ObdParse::ERR_NONE;   // error kind of the last query()
static bool cacheMqtt = false;
static size_t cacheFsUsed = 0, cacheFsTotal = 0;   // LittleFS.usedBytes() walks the FS → refreshed once a minute
static String pendingConfig;          // under mtx (from the web task)
static bool bleOn = true;              // false = safe mode (no Bluetooth)
static volatile bool paused = false;  // firmware update running
static volatile bool bleOffRequested = false, bleIsOff = false;

// Job
struct Job {
  JobType type;
  int car = 0;
  String header, cmd, formula;
  float cap = 0, cons = 0;
  String state = "idle";
  String resultJson;
  String progress;
};
static Job job;
static uint32_t lastJobMs = 0;      // keep connection for another 60 s after test/terminal
static void runJob(bool quickOnly = false);   // defined further down, also called between PIDs

// ---------------------------------------------------------------

static String carName(int i) {
  const CarConfig& c = cfg.cars[i];
  return c.name.length() ? c.name : String("Car ") + (i + 1);
}

// Log line with the car name in front, e.g. "[IONIQ 5] soc = 66.5 %"
static void clog(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void clog(const char* fmt, ...) {
  char buf[240];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  logf("[%s] %s", carName(C->idx).c_str(), buf);
}

static void setError(const String& e) {
  clog("Error: %s", e.c_str());
  {
    std::lock_guard<std::mutex> lk(mtx);
    C->lastError = e;
  }
  MqttHa::publishText(C->idx, "last_error", e);
}

static void reloadProfile(int i) {
  Car& c = cars[i];
  const String& name = cfg.cars[i].profile;
  Profile p;
  bool ok = loadProfile(name, p);
  {
    std::lock_guard<std::mutex> lk(mtx);
    if (ok) profiles[i] = p;
    c.states.assign(profiles[i].pids.size(), PidState());
  }
  if (!cfg.cars[i].enabled) return;
  if (!ok) {
    Car* prev = C;
    C = &c;
    setError(String("Profile '") + name + "' could not be loaded");
    C = prev;
  } else logf("[%s] Profile '%s' loaded (%u PIDs)", carName(i).c_str(), name.c_str(), (unsigned)p.pids.size());
  c.elmReady = false;           // init commands may have changed
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

static void dropLink() {
  ElmBle::disconnect();
  for (auto& c : cars) c.elmReady = false;
}

static bool ensureReady(String& err) {
  if (!linkUp()) {
    for (auto& c : cars) c.elmReady = false;   // the other car's dongle (if any) gets disconnected
    ElmBle::quiet = C->carAsleep;              // connect every minute while asleep → don't log each time
    if (!ElmBle::connect(ccfg(), err)) return false;
    std::lock_guard<std::mutex> lk(mtx);
    C->lastConnectOk = millis();
  }
  if (!C->elmReady) {
    String r;
    // In sleep mode we connect every minute only to read the voltage → no [diag] lines then
    bool dbg = debugOn() && !ElmBle::quiet;
    Watchdog::step("ELM init");
    for (auto& c : prof().init) {
      bool ok = elmCmd(c, r, c.startsWith("ATZ") || c.startsWith("AT Z") ? 4000 : 0);
      if (!linkUp()) {   // connection lost → abort init immediately instead of "no prompt" on every line
        err = String("Connection dropped during initialisation (at ") + c + ")";
        return false;
      }
      if (!ok) clog("Init '%s' without prompt (response: %s)", c.c_str(), oneLine(r).c_str());
      else if (rejected(r)) clog("Init '%s' rejected by the dongle (response: %s)", c.c_str(), oneLine(r).c_str());
      else if (dbg) clog("[diag] %s → %s", c.c_str(), oneLine(r, 60).c_str());
      if (c.startsWith("ATZ")) delay(300);
    }
    C->currentHeader = "";
    // Warm-up: many ELM clones return "CAN ERROR" or truncated responses
    // on the first CAN message right after reset.
    delay(300);
    elmCmd("ATRV", r);
    if (dbg) {
      String dp;
      elmCmd("ATDPN", dp);
      clog("[diag] ATRV → %s, ATDPN → %s, MTU %u", oneLine(r, 20).c_str(), oneLine(dp, 20).c_str(), (unsigned)ElmBle::mtu());
    }
    if (elmCmd("ATI", r) && r.length() && r.length() < 40) {   // e.g. "ELM327 v1.5"
      r.replace("\r", " "); r.trim();
      std::lock_guard<std::mutex> lk(mtx);
      C->elmVersion = r;
    }
    if (!linkUp()) { err = "Connection dropped during initialisation"; return false; }
    C->elmReady = true;
    C->needWarmup = true;
    Watchdog::step("Poller");
    MqttHa::publishText(C->idx, "car", "online");
    MqttHa::publishValue(C->idx, "ble_rssi", ElmBle::rssi(), 0);
  }
  return true;
}

// Custom flow control (ATFCSM1) in the profile? Then the flow control address must follow
// every address change (e.g. IONIQ: 7E4 for the BMS, 7C6 for the odometer).
static bool userFlowControl() {
  for (auto& c : prof().init) {
    String u = c;
    u.toUpperCase();
    u.replace(" ", "");
    if (u == "ATFCSM1") return true;
  }
  return false;
}

static bool setHeader(const String& h) {
  if (h.isEmpty() || h == C->currentHeader) return true;
  String r;
  if (!elmCmd("ATSH" + h, r) || r.indexOf("OK") < 0) {
    C->currentHeader = "";
    return false;
  }
  if (userFlowControl() && (!elmCmd("ATFCSH" + h, r) || r.indexOf("OK") < 0)) {
    C->currentHeader = "";
    return false;
  }
  C->currentHeader = h;
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
    err = linkUp() ? "ATSH" + header + " failed" : String("BLE disconnected");
    return false;
  }
  for (int attempt = 0; attempt < 3; attempt++) {   // retry up to 2× on incomplete response
    if (attempt) {
      String r = raw.substring(0, 80);
      r.replace("\r", "|");
      clog("%s: %s – retrying (raw: %s)", cmd.c_str(), err.c_str(), r.c_str());
      delay(500);
    }
    lastQueryKind = ObdParse::ERR_OTHER;
    if (!elmCmd(cmd, raw)) {
      err = linkUp() ? "Timeout (no prompt)" : "BLE disconnected";
      if (!linkUp()) return false;
      continue;
    }
    std::string e;
    bool ok = ObdParse::parseResponse(raw.c_str(), cmd.c_str(), bytes, e);
    err = e.c_str();
    lastQueryKind = ok ? ObdParse::ERR_NONE : ObdParse::lastErrKind;
    if (debugOn()) clog("[diag] %s %s → %s%s", header.c_str(), cmd.c_str(), oneLine(raw, 400).c_str(), ok ? "" : (String(" [") + err + "]").c_str());
    if (ok) return true;
    // Retry only on incomplete response or CAN error (typical shortly after connecting),
    // not on NO DATA / negative response
    bool retry = ObdParse::lastErrKind == ObdParse::ERR_INCOMPLETE ||
                 (ObdParse::lastErrKind == ObdParse::ERR_CAN && attempt == 0);
    if (!retry) return false;
  }
  return false;
}

// Formula context of the current car: battery values (CAP/CONS) and the last values of its PIDs
// (formulas may use other PIDs of the same car by ID)
static void syncValues() {
  std::vector<std::pair<std::string, double>> v;
  std::lock_guard<std::mutex> lk(mtx);
  const Profile& p = prof();
  for (size_t i = 0; i < p.pids.size() && i < C->states.size(); i++)
    v.push_back({p.pids[i].id.c_str(), p.pids[i].enabled ? C->states[i].value : NAN});
  ObdParse::setValues(v);
  ObdParse::setBattery(p.batteryKwh, p.consumption);
}

// Interval of a PID: the charging / normal interval from the settings; PIDs with a longer profile
// interval than the normal interval (odometer, SOH) keep their own. 0 = profile interval.
static bool slowAwake(const Car& c) {
  return !c.charging && cfg.awakeSlowSec && c.awakePolls >= cfg.awakeFastPolls;
}
static uint32_t intervalMs(const Car& c, const PidDef& p) {
  uint32_t mode = c.charging ? cfg.chargeIntervalSec : slowAwake(c) ? cfg.awakeSlowSec : cfg.normalIntervalSec;
  if (!mode || (cfg.normalIntervalSec && p.interval > cfg.normalIntervalSec)) return p.interval * 1000UL;
  return mode * 1000UL;
}

// Charging state from the last values (PID ids "charging" / "charging_dc"); logs changes
static void refreshCharging(Car& c) {
  const Profile& prof = profiles[c.idx];
  bool ch = false;
  for (size_t i = 0; i < prof.pids.size() && i < c.states.size(); i++) {
    const PidDef& p = prof.pids[i];
    if (p.enabled && (p.id == "charging" || p.id == "charging_dc") && !isnan(c.states[i].value) && c.states[i].value >= 0.5) ch = true;
  }
  if (ch == c.charging) return;
  c.charging = ch;
  c.awakePolls = 0;   // fast polls again after charging starts or stops
  uint32_t iv = ch ? cfg.chargeIntervalSec : cfg.normalIntervalSec;
  if (iv) logf("[%s] %s – polling every %lu s", carName(c.idx).c_str(), ch ? "Charging" : "Not charging", (unsigned long)iv);
}

static bool isDue(Car& c, size_t i, uint32_t now) {
  const PidDef& p = profiles[c.idx].pids[i];
  if (!p.enabled || i >= c.states.size()) return false;
  const PidState& s = c.states[i];
  return s.lastPoll == 0 || now - s.lastPoll >= intervalMs(c, p);
}

static uint32_t msUntilNextDue(Car& c, uint32_t now) {
  uint32_t best = UINT32_MAX;
  const Profile& prof = profiles[c.idx];
  for (size_t i = 0; i < prof.pids.size() && i < c.states.size(); i++) {
    const PidDef& p = prof.pids[i];
    if (!p.enabled) continue;
    const PidState& s = c.states[i];
    if (s.lastPoll == 0) return 0;
    uint32_t el = now - s.lastPoll, iv = intervalMs(c, p);
    best = min(best, el >= iv ? 0 : iv - el);
  }
  return best;
}

// Time until the car is served next and why: off (polling switched off), check (asleep: 12 V check only),
// quick (quick retry after a failed setup), retry (dongle not found), poll (next values due)
static uint32_t msUntilNextPoll(Car& c, uint32_t now, const char*& why) {
  const CarConfig& cc = cfg.cars[c.idx];
  if (!cc.pollEnabled) { why = "off"; return UINT32_MAX; }
  int32_t wait = (int32_t)(c.nextConnectTry - now);
  uint32_t due = msUntilNextDue(c, now);
  uint32_t ms = wait > 0 ? max((uint32_t)wait, due == UINT32_MAX ? 0 : due) : due;
  if (c.carAsleep && cfg.sleepVoltage > 0) why = "check";
  else if (wait > 0 && c.quickRetries) why = "quick";
  else if (wait > 0 && !c.lastError.isEmpty()) why = "retry";
  else why = "poll";
  return ms;
}

// <car base>/next_poll (HA timestamp): when the car is served next ("None" = polling off)
static void publishNextPoll(Car& c) {
  if (!cfg.cars[c.idx].enabled || !timeValid()) return;
  const char* why;
  uint32_t ms = msUntilNextPoll(c, millis(), why);
  if (ms == UINT32_MAX) { MqttHa::publishText(c.idx, "next_poll", "None"); return; }
  time_t t = time(nullptr) + ms / 1000;
  struct tm tmv;
  gmtime_r(&t, &tmv);
  char tb[32];
  strftime(tb, sizeof(tb), "%Y-%m-%dT%H:%M:%S+00:00", &tmv);
  MqttHa::publishText(c.idx, "next_poll", tb);
}

// All current values incl. profile as one JSON: <car base>/state
static void publishStateJson() {
  JsonDocument d;
  const Profile& p = prof();
  d["car"] = carName(C->idx);
  d["profile"] = p.name.length() ? p.name : ccfg().profile;
  d["profile_id"] = ccfg().profile;
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
    for (size_t i = 0; i < p.pids.size() && i < C->states.size(); i++) {
      if (!p.pids[i].enabled || isnan(C->states[i].value)) continue;
      v[p.pids[i].id] = serialized(String(C->states[i].value, p.pids[i].precision));
    }
    if (!isnan(C->lastVoltage)) d["voltage_12v"] = serialized(String(C->lastVoltage, 1));
  }
  String s;
  serializeJson(d, s);
  MqttHa::publishText(C->idx, "state", s);
}

static void pollDue() {
  uint32_t now = millis();
  String err;
  const uint32_t drops0 = ElmBle::dropCount();
  const Profile& profile = prof();
  if (!ensureReady(err)) {
    MqttHa::publishText(C->idx, "car", "offline");
    if (!cfg.keepConnected || !linkUp()) dropLink();
    const bool asleep = C->carAsleep && cfg.sleepVoltage > 0;
    // Dongle answered but the setup failed (e.g. subscribe): it is in range → up to 2 quick retries
    if (ElmBle::lastFailInRange() && C->quickRetries < 2) {
      C->quickRetries++;
      C->nextConnectTry = millis() + 60000UL;
      clog("BLE: %s – dongle in range, retry in 60 s (%u/2)", err.c_str(), (unsigned)C->quickRetries);
      std::lock_guard<std::mutex> lk(mtx);
      C->lastError = err;
      return;
    }
    C->quickRetries = 0;
    C->nextConnectTry = millis() + (asleep ? cfg.sleepCheckMin : cfg.dongleRetryMin) * 60000UL;
    // When the car sleeps, the dongle (e.g. WiCAN) often sleeps too → don't repeat the same message every time
    if (asleep) {
      if (!C->unreachLogged)
        clog("BLE: %s – dongle probably sleeps with the car, checking every %lu min (not reported again)", err.c_str(), (unsigned long)cfg.sleepCheckMin);
      C->unreachLogged = true;
    } else
      clog("BLE: %s – retry in %lu min", err.c_str(), (unsigned long)cfg.dongleRetryMin);
    std::lock_guard<std::mutex> lk(mtx);
    C->lastError = err;
    return;
  }
  C->quickRetries = 0;
  if (C->unreachLogged) {
    C->unreachLogged = false;
    clog("%s", "Dongle reachable again");
  }

  // 12V protection
  float v = readVoltage();
  {
    std::lock_guard<std::mutex> lk(mtx);
    C->lastVoltage = v;
  }
  if (!isnan(v)) MqttHa::publishValue(C->idx, "voltage_12v", v, 1);
  if (cfg.minVoltage > 0 && !isnan(v) && v < cfg.minVoltage) {
    setError(String("12V too low (") + String(v, 1) + " V) – polling paused");
    {
      std::lock_guard<std::mutex> lk(mtx);
      for (size_t i = 0; i < C->states.size(); i++) if (isDue(*C, i, now)) C->states[i].lastPoll = now;
    }
    dropLink();
    C->nextConnectTry = now + max(cfg.backoffSec, (uint32_t)900) * 1000UL;
    return;
  }

  // Sleep mode: if the 12V voltage is below the threshold, the car is off and not charging (no DC/DC converter).
  // Any CAN request would wake it – so only watch the voltage and don't poll anything.
  bool force = C->forcePoll;
  C->forcePoll = false;
  // Voltage unreadable: keep the last state (better not to poll than to wake the car)
  if (cfg.sleepVoltage > 0 && isnan(v) && C->carAsleep) {
    if (!cfg.keepConnected) dropLink();
    C->nextConnectTry = now + cfg.sleepCheckMin * 60000UL;
    return;
  }
  // 12V warning (with 0.2 V hysteresis), independent of sleep mode
  if (cfg.lowBattVoltage > 0 && !isnan(v)) {
    int ls = C->lowState;
    if (v < cfg.lowBattVoltage) ls = 1;
    else if (v >= cfg.lowBattVoltage + 0.2f) ls = 0;
    else if (ls < 0) ls = 0;
    if (ls != C->lowState) {
      C->lowState = ls;
      MqttHa::publishText(C->idx, "battery_low", ls ? "ON" : "OFF");
      if (ls) clog("WARNING: 12 V battery low (%.1f V < %.1f V)", v, cfg.lowBattVoltage);
    }
  }
  if (cfg.sleepVoltage > 0 && !isnan(v)) {
    // Hysteresis like WiCAN: sleep below the threshold, awake only from threshold + 0.1 V
    bool asleep = C->carAsleep ? (v < cfg.sleepVoltage + 0.1f) : (v < cfg.sleepVoltage);
    if (asleep != C->carAsleep) {
      C->carAsleep = asleep;
      C->awakePolls = 0;
      ElmBle::quiet = asleep;
      MqttHa::publishText(C->idx, "car_awake", asleep ? "OFF" : "ON");
      if (asleep) {
        clog("Car asleep (12V %.1f V < %.1f V) – no queries so it can sleep", v, cfg.sleepVoltage);
        // A sleeping car isn't charging: set charging values to 0, otherwise HA would keep the last value
        bool any = false;
        for (size_t i = 0; i < profile.pids.size() && i < C->states.size(); i++) {
          const PidDef& p = profile.pids[i];
          if (!p.enabled || !p.sleepZero) continue;
          {
            std::lock_guard<std::mutex> lk(mtx);
            C->states[i].value = 0;
          }
          MqttHa::publishValue(C->idx, p.id, 0, p.precision);
          any = true;
        }
        if (any) publishStateJson();
      }
      else
        clog("Car awake (12V %.1f V) – normal polling", v);
    }
    if (asleep) {
      if (force) clog("“Poll now” – car is asleep (12V %.1f V), only the voltage was checked – not woken", v);
      // Wake periodically only if the 12 V battery can handle it (like WiCAN: above 11.9 V)
      bool sleepPollDue = cfg.sleepPollMin > 0 && v > 11.9f &&
                          (C->lastSleepPoll == 0 || now - C->lastSleepPoll >= cfg.sleepPollMin * 60000UL);
      if (!sleepPollDue) {
        if (!cfg.keepConnected) dropLink();
        C->nextConnectTry = now + cfg.sleepCheckMin * 60000UL;   // check only the voltage again later (doesn't wake the car)
        return;
      }
      C->lastSleepPoll = now;
      clog("Polling while asleep (every %lu min)", (unsigned long)cfg.sleepPollMin);
    }
  }

  // Warm-up (only if the car is awake or may be woken): the first long message after
  // connecting often arrives truncated (IONIQ gateway). A short "Tester Present" (3E00) to the
  // ECU of the first query wakes up the path beforehand – the response is ignored.
  if (C->needWarmup) {
    C->needWarmup = false;
    for (size_t i = 0; i < profile.pids.size(); i++) {
      if (!isDue(*C, i, now) || profile.pids[i].header.isEmpty()) continue;
      Watchdog::step("warm-up 3E00");
      if (setHeader(profile.pids[i].header)) {
        String r;
        elmCmd("3E00", r, 1500);
        if (debugOn()) clog("[diag] %s 3E00 → %s", profile.pids[i].header.c_str(), oneLine(r, 60).c_str());
        delay(100);
      }
      Watchdog::step("Poller");
      break;
    }
  }

  // Poll due PIDs. Cache responses per cycle: send identical header+cmd only once,
  // even if the PIDs aren't adjacent
  struct QRes { bool ok; std::vector<uint8_t> bytes; String raw, err; };
  std::map<String, QRes> cache;
  std::vector<uint8_t> bytes;
  String raw, qerr;
  bool qok = false, anyOk = false, anyTried = false, anyReply = false, anyErr = false;
  bool firstQuery = true, stopCycle = false;
  for (size_t i = 0; i < profile.pids.size(); i++) {
    if (!isDue(*C, i, now)) continue;
    const PidDef& p = profile.pids[i];
    String key = p.header + "|" + p.cmd;
    auto it = cache.find(key);
    if (it == cache.end()) {
      Watchdog::step(("query " + p.cmd).c_str());
      qok = query(p.header, p.cmd, bytes, raw, qerr);
      Watchdog::step("Poller");
      // car responded (even if incomplete/negative) → it's awake, no sleep pause
      if (qok || lastQueryKind == ObdParse::ERR_INCOMPLETE || lastQueryKind == ObdParse::ERR_NRC) anyReply = true;
      // First query of the cycle answers NO DATA (e.g. the battery management of a sleeping car):
      // don't query the other ECUs (odometer, tyres …) – their answers could keep the car awake
      if (firstQuery && !qok && lastQueryKind == ObdParse::ERR_NO_DATA) stopCycle = true;
      firstQuery = false;
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
      PidState& s = C->states[i];
      s.lastPoll = now;
      s.raw = raw.substring(0, 300);
      s.err = ok ? "" : perr;
      if (ok) { s.value = val; s.lastOk = now; }
    }
    anyTried = true;
    anyOk |= ok;
    if (ok) {
      MqttHa::publishValue(C->idx, p.id, val, p.precision);
      clog("%s = %s %s", p.id.c_str(), String(val, p.precision).c_str(), p.unit.c_str());
    } else {
      setError(p.id + ": " + perr);
      anyErr = true;
    }
    if (stopCycle) {
      clog("%s: NO DATA – car probably asleep, other queries skipped this cycle", p.id.c_str());
      break;
    }
    if (!linkUp()) { C->elmReady = false; break; }
    runJob(true);   // don't make Test/Terminal wait for the whole cycle
  }
  C->pollCycles++;
  // "Last data": time of the last cycle that delivered at least one valid value (HA timestamp sensor).
  // Restarts of the bridge don't change it – unlike HA's "last changed" of the values.
  if (anyOk) {
    {
      std::lock_guard<std::mutex> lk(mtx);
      C->lastDataMs = millis();
    }
    if (timeValid()) {
      time_t tnow = time(nullptr);
      struct tm tmv;
      gmtime_r(&tnow, &tmv);
      char tb[32];
      strftime(tb, sizeof(tb), "%Y-%m-%dT%H:%M:%S+00:00", &tmv);
      MqttHa::publishText(C->idx, "last_data", tb);
    }
  }
  // Cycle without errors → reset "last error" and always publish
  // (MQTT is retained: otherwise the old error would remain in the broker after a reboot)
  if (anyTried && !anyErr) {
    {
      std::lock_guard<std::mutex> lk(mtx);
      C->lastError = "";
    }
    MqttHa::publishText(C->idx, "last_error", "none");
  }
  publishStateJson();
  // Connection lost during polling: not a sign of a sleeping car → retry after 60 s
  if (ElmBle::dropCount() != drops0) {
    C->elmReady = false;
    C->nextConnectTry = millis() + 60000UL;
    clog("%s", "BLE connection dropped during polling – retry in 60s");
    {
      // failed PIDs due again on the next attempt (unpolled ones still are anyway)
      std::lock_guard<std::mutex> lk(mtx);
      for (auto& s : C->states) if (s.lastPoll == now && !s.err.isEmpty()) s.lastPoll = 0;
    }
    return;
  }
  // No response (car asleep) → wait longer so nothing is kept awake
  if (anyTried && !anyOk && !anyReply && cfg.backoffSec > 0) {
    C->nextConnectTry = millis() + cfg.backoffSec * 1000UL;
    clog("No valid response – next attempt in %lus", (unsigned long)cfg.backoffSec);
    if (!cfg.keepConnected) dropLink();
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

// Diagnostic report of the current car: fresh connection, every init response, every query with raw
// data and bytes. Deliberately contains no Wi-Fi/IP/password data so it can be posted publicly.
static String diagReport() {
  String o;
  o.reserve(8000);
  auto ln = [&](const String& s) { o += s; o += '\n'; };
  const Profile& profile = prof();
  const CarConfig& cc = ccfg();
  ln("=== OBD2MQTT " + String("diagnostic report") + " ===");
  ln("Firmware: " FW_VERSION);
  if (timeValid()) {
    time_t tn = time(nullptr); struct tm tmv; localtime_r(&tn, &tmv);
    char tb[32]; strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M:%S", &tmv);
    ln(String("Time: ") + tb);
  }
  ln(String("Car: ") + (C->idx + 1) + " – " + carName(C->idx));
  ln(String("Profile: ") + cc.profile + " – " + profile.name + " (" + profile.model + ")");
  String addr = ElmBle::currentAddress(cc);
  String mac = addr.length() == 17 ? addr.substring(0, 8) + ":xx:xx:xx" : addr;
  if (cc.bleMatch == "name") mac = "found by name, " + (mac.length() ? mac : String("not found yet"));
  ln("Dongle: " + ElmBle::deviceName(cc) + " / " + C->cacheKind + " / " + mac);
  ln(String("Last restart: ") + Watchdog::resetReason() +
     (strlen(Watchdog::hangStep()) ? String(" – hung at: ") + Watchdog::hangStep() : String("")));
  ln(String("BLE connection parameters: ") +
     (ElmBle::relaxedParams(cc) ? "slow (after repeated 0x208)" : "fast (7.5–15 ms)") +
     ", " + "drops since boot: " + String(ElmBle::dropCount()));
  if (!isnan(C->lastVoltage)) ln(String("12V (last reading): ") + String(C->lastVoltage, 1) + " V");

  setProgress("BLE");
  Watchdog::step("diag report");
  dropLink();
  C->currentHeader = "";
  String err;
  // up to 3 attempts – a single glitch shouldn't make the whole report worthless
  bool con = false;
  for (int a = 0; a < 3 && !con; a++) {
    Watchdog::feed();
    if (a) { ln(String("BLE attempt ") + a + " failed: " + err); delay(2000); }
    err = "";
    con = ElmBle::connect(cc, err);
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
    if (!linkUp()) { ln("!!! BLE connection dropped – report incomplete"); Watchdog::step("Poller"); return o; }
  }
  delay(300);
  for (const char* c : {"ATI", "AT@1", "ATRV", "ATDPN"}) {
    Watchdog::feed();
    String r;
    elmCmd(c, r);
    ln(String(c) + " → " + oneLine(r, 80));
  }
  C->elmReady = true;
  {
    std::lock_guard<std::mutex> lk(mtx);
    C->lastConnectOk = millis();
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
    if (!linkUp()) { ln(""); ln("!!! BLE connection dropped – remaining queries skipped"); break; }
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
  ln(String("BLE: ") + (linkUp() ? String("RSSI ") + ElmBle::rssi() + " dBm" : String("connection lost at the end")) +
     ", " + "drops since boot: " + String(ElmBle::dropCount()));
  ln(String("Init problems: ") + initBad);
  ln(String("Queries OK: ") + nOk + " / " + keys.size() + ", " + "incomplete: " + nInc +
     ", " + "no answer/error: " + nFail);
  return o;
}

static void runJob(bool quickOnly) {
  Job j;
  {
    std::lock_guard<std::mutex> lk(mtx);
    if (job.state != "pending") return;
    // between PIDs only terminal/test jobs for the car being polled; scan and diagnostics
    // (or the other car) would tear down the connection
    if (quickOnly && (job.type == JOB_SCAN || job.type == JOB_DIAG || job.car != C->idx)) return;
    job.state = "running";
    j = job;
  }
  Car* prev = C;
  C = &cars[j.car];
  JsonDocument r;
  if (j.type == JOB_DIAG) {
    clog("%s", "Creating diagnostic report…");
    String rep = diagReport();
    r["len"] = rep.length();
    {
      std::lock_guard<std::mutex> lk(mtx);
      diagText = "";          // free the old report first
      diagText = std::move(rep);
    }
    r["ok"] = true;
    clog("%s", "Diagnostic report done");
  } else if (j.type == JOB_SCAN) {
    Watchdog::step("BLE scan");
    dropLink();
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
      if (up.startsWith("AT")) C->currentHeader = "";   // header may have been changed manually
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
          double v;
          std::string e;
          syncValues();
          // battery values from the profile editor (not saved yet) take precedence
          if (j.cap > 0 || j.cons > 0)
            ObdParse::setBattery(j.cap > 0 ? j.cap : prof().batteryKwh, j.cons > 0 ? j.cons : prof().consumption);
          bool fo = ObdParse::evalFormula(j.formula.c_str(), bytes, v, e);
          if (fo) r["value"] = v;
          else { ok = false; err = e.c_str(); }
        }
      }
      r["ok"] = ok;
      if (!ok) r["error"] = err;
    }
  }
  C = prev;
  String out;
  serializeJson(r, out);
  lastJobMs = millis();
  std::lock_guard<std::mutex> lk(mtx);
  job.resultJson = out;
  job.state = "done";
}

bool submitJob(JobType t, int car, const String& header, const String& cmd, const String& formula, float cap, float cons) {
  std::lock_guard<std::mutex> lk(mtx);
  if (job.state == "pending" || job.state == "running") return false;
  job.type = t;
  job.car = constrain(car, 0, NCARS - 1);
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

static bool jobBusy() {
  std::lock_guard<std::mutex> lk(mtx);
  return job.state == "pending" || job.state == "running";
}

// ---------------------------------------------------------------

void requestConfigApply(const String& json) {
  std::lock_guard<std::mutex> lk(mtx);
  pendingConfig = json;
}
void requestProfileReload() { for (auto& c : cars) c.flagReload = true; }
String lastErrorText(int car) {
  std::lock_guard<std::mutex> lk(mtx);
  return cars[constrain(car, 0, NCARS - 1)].lastError;
}

void requestPollNow(int car) {
  for (auto& c : cars)
    if ((car < 0 || c.idx == car) && cfg.cars[c.idx].enabled) {
      c.forcePoll = true;
      c.flagPollNow = true;
      logf("[%s] Poll now (Home Assistant / web)", carName(c.idx).c_str());
    }
}

void requestCheckNow(int car) { cars[constrain(car, 0, NCARS - 1)].flagCheckNow = true; }

void requestEnabled(int car, bool on) { cars[constrain(car, 0, NCARS - 1)].pendingEnable = on ? 1 : 0; }

static void applyEnable() {
  for (auto& c : cars) {
    int pe = c.pendingEnable;
    if (pe < 0) continue;
    c.pendingEnable = -1;
    bool on = pe == 1;
    MqttHa::publishText(c.idx, "polling", on ? "ON" : "OFF");
    if (cfg.cars[c.idx].pollEnabled == on) continue;
    cfg.cars[c.idx].pollEnabled = on;
    saveConfig();
    logf("[%s] %s", carName(c.idx).c_str(), on ? "Polling switched on" : "Polling switched off – no connection to the car");
    if (on) c.nextConnectTry = 0;
    publishNextPoll(c);
  }
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
  for (int i = 0; i < NCARS; i++) {
    const CarConfig &b = before.cars[i], &a = cfg.cars[i];
    if (b.bleMac != a.bleMac || b.bleMatch != a.bleMatch || b.bleName != a.bleName || b.bleService != a.bleService || b.bleNotify != a.bleNotify ||
        b.bleWrite != a.bleWrite || b.bleAddrType != a.bleAddrType || b.blePin != a.blePin || b.enabled != a.enabled) {
      if (ElmBle::connectedTo(a)) dropLink();
      cars[i].elmReady = false;
      cars[i].nextConnectTry = 0;
    }
    if (b.profile != a.profile || (a.enabled && !b.enabled)) reloadProfile(i);
    if (b.deviceName != a.deviceName || b.mqttBase != a.mqttBase || b.enabled != a.enabled || b.name != a.name)
      MqttHa::requestDiscovery();
  }
  if (before.sleepVoltage != cfg.sleepVoltage || before.lowBattVoltage != cfg.lowBattVoltage) MqttHa::requestDiscovery();
  if (before.bleTxPower != cfg.bleTxPower && bleOn && !paused) {
    ElmBle::setTxPower(cfg.bleTxPower);
    logf("Bluetooth transmit power %d dBm", cfg.bleTxPower);
  }
  // Wi-Fi/MQTT changes take effect via reboot from the web UI
}

void begin(bool bleEnabled) {
  for (int i = 0; i < NCARS; i++) cars[i].idx = i;
  bleOn = bleEnabled;
  if (bleOn) ElmBle::begin();
  for (int i = 0; i < NCARS; i++) reloadProfile(i);
}

static bool pollable(Car& c);
static void updateCache() {
  bool m = MqttHa::connected();
  static uint32_t lastInfo = 0, lastFs = 0;
  bool info = !lastInfo || millis() - lastInfo > 2000;   // RSSI (controller round trip) and texts only every 2 s
  bool fs = !lastFs || millis() - lastFs > 60000;
  int r = 0, connectedCar = -1;
  String nm[NCARS], ad[NCARS], kd, uu;
  for (int i = 0; i < NCARS; i++) if (ElmBle::connectedTo(cfg.cars[i])) connectedCar = i;
  if (info) {
    lastInfo = millis();
    if (connectedCar >= 0) { r = ElmBle::rssi(); kd = ElmBle::dongleKind(); uu = ElmBle::detectedUuids(); }
    for (int i = 0; i < NCARS; i++) { nm[i] = ElmBle::deviceName(cfg.cars[i]); ad[i] = ElmBle::currentAddress(cfg.cars[i]); }
  }
  size_t fu = 0, ft = 0;
  if (fs) { lastFs = millis(); fu = LittleFS.usedBytes(); ft = LittleFS.totalBytes(); }
  std::lock_guard<std::mutex> lk(mtx);
  cacheMqtt = m;
  for (auto& c : cars) {
    c.cacheBle = c.idx == connectedCar;
    if (!c.cacheBle) c.cacheRssi = 0;
    if (info) {
      c.cacheName = nm[c.idx];
      c.cacheAddr = ad[c.idx];
      if (c.cacheBle) { c.cacheRssi = r; c.cacheKind = kd; c.cacheUuids = uu; }
    }
  }
  if (fs) { cacheFsUsed = fu; cacheFsTotal = ft; }
  // Status LED: activity of the connection, otherwise a summary of the cars
  Led::setActivity(connectedCar >= 0 ? Led::CONNECTED : Led::IDLE);
  bool fault = false, asleep = false;
  for (auto& c : cars) {
    if (!pollable(c)) continue;
    bool sl = c.carAsleep && cfg.sleepVoltage > 0;
    asleep |= sl;
    fault |= !sl && !c.lastError.isEmpty();
  }
  Led::setCars(fault, asleep);
}

bool isPaused() { return paused || !bleOn; }
bool bleStopped() { return bleIsOff; }
void setPaused(bool p, bool bleOff) {
  if (p && bleOff) bleOffRequested = true;
  paused = p;
  Led::setUpdate(p);
  if (p) logf("%s", "Polling paused (firmware update)");
}

// Car that may be polled automatically (enabled, dongle configured, polling on or "Poll now" pending)
static bool pollable(Car& c) {
  const CarConfig& cc = cfg.cars[c.idx];
  return cc.enabled && ElmBle::configured(cc) && (cc.pollEnabled || c.flagPollNow);
}

void loop() {
  applyEnable();
  if (!bleOn || paused) {
    if (paused && ElmBle::connected()) dropLink();
    // Firmware upload: switch the Bluetooth stack off completely – less current while WiFi and flash are busy
    // (brownouts during uploads). Done here in the loop task, never while a BLE command is running.
    if (paused && bleOn && bleOffRequested && !bleIsOff) {
      ElmBle::shutdown();
      bleIsOff = true;
      logf("%s", "Bluetooth switched off for the firmware update");
    }
    if (!paused) applyPendingConfig();
    for (int i = 0; i < NCARS; i++) if (cars[i].flagReload) { cars[i].flagReload = false; reloadProfile(i); }
    updateCache();
    return;
  }
  applyPendingConfig();
  for (int i = 0; i < NCARS; i++) if (cars[i].flagReload) { cars[i].flagReload = false; reloadProfile(i); }
  runJob();

  uint32_t now = millis();
  // "Check now" (HA button): next voltage check right away. Unlike "Poll now" the sleep check stays active,
  // so a sleeping car is not woken – an awake (e.g. AC charging) car is polled immediately.
  for (auto& c : cars) {
    if (!c.flagCheckNow) continue;
    c.flagCheckNow = false;
    if (!pollable(c)) continue;
    std::lock_guard<std::mutex> lk(mtx);
    for (auto& s : c.states) s.lastPoll = 0;
    c.nextConnectTry = 0;
    c.awakePolls = 0;
    logf("[%s] Check now (Home Assistant / web)", carName(c.idx).c_str());
  }
  for (auto& c : cars) {
    if (!c.flagPollNow) continue;
    if (!pollable(c)) { c.flagPollNow = false; c.forcePoll = false; continue; }   // disabled / no dongle
    std::lock_guard<std::mutex> lk(mtx);
    for (auto& s : c.states) s.lastPoll = 0;
    c.nextConnectTry = 0;
    c.awakePolls = 0;
  }

  // Pick the car to serve: the connected car first (finish it), then round-robin from the last served car
  static int lastServed = NCARS - 1;
  int pick = -1;
  for (int k = 0; k <= NCARS && pick < 0; k++) {
    int i = k == 0 ? -1 : (lastServed + k) % NCARS;
    if (k == 0) {
      for (int j = 0; j < NCARS; j++) if (ElmBle::connectedTo(cfg.cars[j])) i = j;
      if (i < 0) continue;
    }
    Car& c = cars[i];
    if (pollable(c) && msUntilNextDue(c, now) == 0 && (int32_t)(now - c.nextConnectTry) >= 0) pick = i;
  }
  if (pick >= 0) {
    C = &cars[pick];
    Car& pc = cars[pick];
    pc.flagPollNow = false;
    const uint32_t data0 = pc.lastDataMs;
    pollDue();
    refreshCharging(pc);
    // Awake, not charging: count the polls with data; after awakeFastPolls switch to the slow interval
    if (pc.lastDataMs != data0 && !pc.charging && !(pc.carAsleep && cfg.sleepVoltage > 0) && pc.awakePolls < 255) {
      pc.awakePolls++;
      if (cfg.awakeSlowSec && pc.awakePolls == cfg.awakeFastPolls)
        logf("[%s] Awake, not charging – polling every %lu min", carName(pick).c_str(), (unsigned long)(cfg.awakeSlowSec / 60));
    }
    publishNextPoll(cars[pick]);
    lastServed = pick;
  }

  // Let the connected dongle sleep when its car has nothing to do for a while
  for (auto& c : cars) {
    if (!ElmBle::connectedTo(cfg.cars[c.idx])) continue;
    bool idle = !pollable(c) || msUntilNextDue(c, millis()) > 20000;
    if ((!cfg.keepConnected || !pollable(c)) && idle && !jobBusy() && (!lastJobMs || millis() - lastJobMs > 60000))
      dropLink();   // dongle may sleep
  }
  updateCache();
}

void statusJson(JsonDocument& d) {
  // Everything that doesn't touch poller state is collected before taking the lock,
  // so the web task holds the mutex only for the short state copy
  uint32_t now = millis();
  d["fw"] = FW_VERSION;
  d["board"] = FW_BOARD;
  d["ble_tx_max"] = BLE_TX_MAX;
  d["uptime"] = now / 1000;
  d["heap"] = ESP.getFreeHeap();
  d["heap_min"] = ESP.getMinFreeHeap();
  d["heap_block"] = ESP.getMaxAllocHeap();
  d["wifi"]["connected"] = WiFi.status() == WL_CONNECTED;
  d["wifi"]["ssid"] = WiFi.SSID();
  d["wifi"]["ip"] = WiFi.localIP().toString();
  d["wifi"]["rssi"] = WiFi.RSSI();
  d["wifi"]["gw"] = WiFi.gatewayIP().toString();
  d["wifi"]["mask"] = WiFi.subnetMask().toString();
  d["wifi"]["dns"] = WiFi.dnsIP(0).toString();
  d["hang_step"] = Watchdog::hangStep();
  d["ble_drops"] = ElmBle::dropCount();
  if (timeValid()) {
    time_t tnow = time(nullptr);
    struct tm tmv;
    localtime_r(&tnow, &tmv);
    char tb[32];
    strftime(tb, sizeof(tb), "%d.%m.%Y %H:%M:%S %Z", &tmv);
    d["time"] = tb;
  }
  d["log_seq"] = logSeq();   // the UI fetches /api/logtail only when this changes
  d["debug_min"] = debugOn() ? (uint32_t)((debugUntil - millis()) / 60000UL) + 1 : 0;

  std::lock_guard<std::mutex> lk(mtx);
  d["fs_used"] = cacheFsUsed;
  d["fs_total"] = cacheFsTotal;
  d["wifi"]["ip_mode"] = g_ipFallback ? "fallback" : cfg.ipMode;
  d["mqtt"] = cacheMqtt;
  JsonArray ca = d["cars"].to<JsonArray>();
  for (auto& c : cars) {
    const CarConfig& cc = cfg.cars[c.idx];
    const Profile& prof = profiles[c.idx];
    JsonObject o = ca.add<JsonObject>();
    o["enabled"] = cc.enabled;
    o["name"] = carName(c.idx);
    o["base"] = cc.mqttBase;
    o["ble"]["connected"] = c.cacheBle;
    o["ble"]["mac"] = c.cacheAddr;
    o["ble"]["by_name"] = cc.bleMatch == "name";
    o["ble"]["rssi"] = c.cacheRssi;
    o["ble"]["uuids"] = c.cacheUuids;
    o["ble"]["name"] = c.cacheName;
    o["ble"]["kind"] = c.cacheKind;
    o["ble"]["elm"] = c.elmVersion;
    o["ble"]["slow_params"] = ElmBle::relaxedParams(cc);
    o["last_data_s"] = c.lastDataMs ? (int32_t)((now - c.lastDataMs) / 1000) : -1;
    o["ble"]["last_ok_s"] = c.lastConnectOk ? (int32_t)((now - c.lastConnectOk) / 1000) : -1;
    o["ble"]["next_try_s"] = (int32_t)(c.nextConnectTry - now) > 0 ? (int32_t)((c.nextConnectTry - now) / 1000) : 0;
    if (!isnan(c.lastVoltage)) o["voltage"] = c.lastVoltage;
    o["car_asleep"] = cfg.sleepVoltage > 0 && c.carAsleep;
    o["poll_enabled"] = cc.pollEnabled;
    o["profile"] = cc.profile;
    o["profile_name"] = prof.name;
    o["last_error"] = c.lastError;
    {
      const char* why;
      uint32_t ms = msUntilNextPoll(c, now, why);
      o["next_poll_s"] = ms == UINT32_MAX ? -1 : (int32_t)(ms / 1000);
      o["next_why"] = why;
      o["charging_mode"] = c.charging;
      o["interval_s"] = c.charging ? cfg.chargeIntervalSec : slowAwake(c) ? cfg.awakeSlowSec : cfg.normalIntervalSec;
      o["quick_retry"] = c.quickRetries;
    }
    o["cycles"] = c.pollCycles;
    JsonArray arr = o["pids"].to<JsonArray>();
    for (size_t i = 0; i < prof.pids.size() && i < c.states.size(); i++) {
      const PidDef& p = prof.pids[i];
      const PidState& s = c.states[i];
      JsonObject po = arr.add<JsonObject>();
      po["id"] = p.id; po["name"] = p.name; po["unit"] = p.unit; po["enabled"] = p.enabled;
      if (!isnan(s.value)) po["value"] = serialized(String(s.value, p.precision));
      po["age_s"] = s.lastOk ? (int32_t)((now - s.lastOk) / 1000) : -1;
      if (s.lastPoll) {
        uint32_t el = now - s.lastPoll, iv = intervalMs(c, p);
        po["next_s"] = el >= iv ? 0 : (iv - el) / 1000;
      } else po["next_s"] = 0;
      po["error"] = s.err;
    }
  }
}

}  // namespace Poller
