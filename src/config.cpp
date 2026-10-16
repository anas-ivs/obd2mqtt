#include "config.h"
#include <LittleFS.h>
#include "default_profiles.h"
#include "log.h"

AppConfig::AppConfig() {
  cars[0].enabled = true;
  cars[0].name = "IONIQ 5";
  cars[0].deviceName = "IONIQ 5";
  cars[0].mqttBase = "obd2mqtt/car1";
  cars[0].profile = "ioniq5";
  cars[1].name = "EV6";
  cars[1].deviceName = "EV6";
  cars[1].mqttBase = "obd2mqtt/car2";
  cars[1].profile = "ev6";
}

AppConfig cfg;
Profile profiles[NCARS];
const char* MASK = "********";

static const char* CONFIG_FILE = "/config.json";

bool fsBegin() {
  if (!LittleFS.begin(true)) return false;   // formats if needed
  if (!LittleFS.exists("/profiles")) LittleFS.mkdir("/profiles");
  installDefaultProfiles(false);
  return true;
}

bool validProfileName(const String& n) {
  if (n.isEmpty() || n.length() > 24) return false;
  for (char c : n)
    if (!(isalnum((unsigned char)c) || c == '_' || c == '-')) return false;
  return true;
}

void installDefaultProfiles(bool overwrite) {
  for (auto& d : DEFAULT_PROFILES) {
    String path = String("/profiles/") + d.file + ".json";
    if (!overwrite && LittleFS.exists(path)) continue;
    File f = LittleFS.open(path, "w");
    if (f) { f.print(FPSTR(d.json)); f.close(); }
  }
}

// ---------------- Config ----------------

void configToJson(JsonDocument& d, bool mask) {
  auto secret = [&](const String& s) { return (mask && s.length()) ? String(MASK) : s; };
  d["wifi_ssid"] = cfg.wifiSsid;
  d["wifi_pass"] = secret(cfg.wifiPass);
  d["hostname"] = cfg.hostname;
  d["web_pass"] = secret(cfg.webPass);
  d["ip_mode"] = cfg.ipMode;
  d["ip_addr"] = cfg.ipAddr;
  d["ip_mask"] = cfg.ipMask;
  d["ip_gw"] = cfg.ipGw;
  d["ip_dns1"] = cfg.ipDns1;
  d["ip_dns2"] = cfg.ipDns2;
  d["mqtt_host"] = cfg.mqttHost;
  d["mqtt_port"] = cfg.mqttPort;
  d["mqtt_user"] = cfg.mqttUser;
  d["mqtt_pass"] = secret(cfg.mqttPass);
  d["mqtt_base"] = cfg.mqttBase;
  d["ha_prefix"] = cfg.haPrefix;
  JsonArray cars = d["cars"].to<JsonArray>();
  for (auto& c : cfg.cars) {
    JsonObject o = cars.add<JsonObject>();
    o["enabled"] = c.enabled;
    o["name"] = c.name;
    o["device_name"] = c.deviceName;
    o["mqtt_base"] = c.mqttBase;
    o["ble_mac"] = c.bleMac;
    o["ble_addr_type"] = c.bleAddrType;
    o["ble_service"] = c.bleService;
    o["ble_notify"] = c.bleNotify;
    o["ble_write"] = c.bleWrite;
    o["ble_name"] = c.bleName;
    o["ble_pin"] = c.blePin;
    o["profile"] = c.profile;
    o["poll_enabled"] = c.pollEnabled;
  }
  d["min_voltage"] = cfg.minVoltage;
  d["sleep_voltage"] = cfg.sleepVoltage;
  d["sleep_poll_min"] = cfg.sleepPollMin;
  d["low_batt_voltage"] = cfg.lowBattVoltage;
  d["retry_sec"] = cfg.retrySec;
  d["cmd_timeout_ms"] = cfg.cmdTimeoutMs;
  d["keep_connected"] = cfg.keepConnected;
  d["backoff_sec"] = cfg.backoffSec;
  d["ntp_server"] = cfg.ntpServer;
  d["tz"] = cfg.tz;
  d["log_persist"] = cfg.logPersist;
  d["update_check"] = cfg.updateCheck;
}

// Car settings from a JSON object; only keys that are present are changed
static void carFromJson(JsonVariantConst s, CarConfig& c) {
  auto str = [&](const char* k, String& dst) {
    if (!s[k].is<const char*>()) return;
    String v = s[k].as<String>();
    v.trim();
    dst = v;
  };
  if (s["enabled"].is<bool>()) c.enabled = s["enabled"];
  str("name", c.name);
  str("device_name", c.deviceName);
  str("mqtt_base", c.mqttBase);
  while (c.mqttBase.endsWith("/")) c.mqttBase.remove(c.mqttBase.length() - 1);
  str("ble_mac", c.bleMac);
  c.bleMac.toUpperCase();
  str("ble_addr_type", c.bleAddrType);
  str("ble_service", c.bleService);
  str("ble_notify", c.bleNotify);
  str("ble_write", c.bleWrite);
  str("ble_name", c.bleName);
  str("ble_pin", c.blePin);
  { String d; for (char ch : c.blePin) if (isdigit((unsigned char)ch)) d += ch; c.blePin = d.substring(0, 6); }
  str("profile", c.profile);
  if (s["poll_enabled"].is<bool>()) c.pollEnabled = s["poll_enabled"];
}

void configFromJson(JsonVariantConst s) {
  auto str = [&](const char* k, String& dst, bool isSecret = false) {
    if (!s[k].is<const char*>()) return;
    String v = s[k].as<String>();
    if (isSecret && v == MASK) return;
    v.trim();
    dst = v;
  };
  str("wifi_ssid", cfg.wifiSsid);
  str("wifi_pass", cfg.wifiPass, true);
  str("hostname", cfg.hostname);
  str("web_pass", cfg.webPass, true);
  str("ip_mode", cfg.ipMode);
  if (cfg.ipMode != "static") cfg.ipMode = "dhcp";
  str("ip_addr", cfg.ipAddr);
  str("ip_mask", cfg.ipMask);
  if (cfg.ipMask.isEmpty()) cfg.ipMask = "255.255.255.0";
  str("ip_gw", cfg.ipGw);
  str("ip_dns1", cfg.ipDns1);
  str("ip_dns2", cfg.ipDns2);
  str("mqtt_host", cfg.mqttHost);
  if (s["mqtt_port"].is<int>()) cfg.mqttPort = s["mqtt_port"];
  str("mqtt_user", cfg.mqttUser);
  str("mqtt_pass", cfg.mqttPass, true);
  str("mqtt_base", cfg.mqttBase);
  str("ha_prefix", cfg.haPrefix);
  if (s["cars"].is<JsonArrayConst>()) {
    // [ {car 1 keys}, {car 2 keys} ] – an entry may be empty/null to leave that car unchanged
    int i = 0;
    for (JsonVariantConst c : s["cars"].as<JsonArrayConst>()) {
      if (i >= NCARS) break;
      if (c.is<JsonObjectConst>()) carFromJson(c, cfg.cars[i]);
      i++;
    }
  } else if (s["ble_mac"].is<const char*>()) {
    // Settings from v0.4 or older (one car, flat keys): they become car 1
    carFromJson(s, cfg.cars[0]);
    if (s["device_name"].is<const char*>()) cfg.cars[0].name = cfg.cars[0].deviceName;
    cfg.cars[0].mqttBase = "";   // the old flat "mqtt_base" is the bridge base → car 1 gets <base>/car1 below
  }
  if (s["min_voltage"].is<float>()) cfg.minVoltage = s["min_voltage"];
  if (s["sleep_voltage"].is<float>()) cfg.sleepVoltage = constrain(s["sleep_voltage"].as<float>(), 0.0f, 16.0f);
  if (s["low_batt_voltage"].is<float>()) cfg.lowBattVoltage = constrain(s["low_batt_voltage"].as<float>(), 0.0f, 15.0f);
  if (s["sleep_poll_min"].is<int>()) cfg.sleepPollMin = max(0, s["sleep_poll_min"].as<int>());
  if (s["retry_sec"].is<int>()) cfg.retrySec = max(10, s["retry_sec"].as<int>());
  if (s["cmd_timeout_ms"].is<int>()) cfg.cmdTimeoutMs = constrain(s["cmd_timeout_ms"].as<int>(), 500, 15000);
  if (s["keep_connected"].is<bool>()) cfg.keepConnected = s["keep_connected"];
  if (s["backoff_sec"].is<int>()) cfg.backoffSec = max(0, s["backoff_sec"].as<int>());
  str("ntp_server", cfg.ntpServer);
  str("tz", cfg.tz);
  if (cfg.ntpServer.isEmpty()) cfg.ntpServer = "pool.ntp.org";
  if (cfg.tz.isEmpty()) cfg.tz = "MYT-8";
  if (s["log_persist"].is<bool>()) cfg.logPersist = s["log_persist"];
  if (s["update_check"].is<bool>()) cfg.updateCheck = s["update_check"];
  if (cfg.hostname.isEmpty()) cfg.hostname = "obd2mqtt";
  if (cfg.mqttBase.isEmpty()) cfg.mqttBase = "obd2mqtt";
  for (int i = 0; i < NCARS; i++)
    if (cfg.cars[i].mqttBase.isEmpty()) cfg.cars[i].mqttBase = cfg.mqttBase + "/car" + String(i + 1);
  if (cfg.haPrefix.isEmpty()) cfg.haPrefix = "homeassistant";
}

bool loadConfig() {
  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f) return false;
  JsonDocument d;
  bool ok = !deserializeJson(d, f);
  f.close();
  if (ok) configFromJson(d.as<JsonVariantConst>());
  return ok;
}

bool saveConfig() {
  JsonDocument d;
  configToJson(d, false);
  File f = LittleFS.open(CONFIG_FILE, "w");
  if (!f) return false;
  serializeJson(d, f);
  f.close();
  return true;
}

// ---------------- Profile ----------------

static bool isHex(const String& s) {
  if (s.isEmpty()) return false;
  for (char c : s) if (!isxdigit((unsigned char)c)) return false;
  return true;
}

bool parseProfile(JsonVariantConst s, Profile& p, String& err) {
  p = Profile();
  p.name = s["name"] | "";
  p.model = s["model"] | "";
  p.batteryKwh = s["battery_kwh"] | 0.0f;
  p.consumption = s["consumption"] | 0.0f;
  for (JsonVariantConst o : s["battery_options"].as<JsonArrayConst>()) {
    BatteryOption b{o["name"] | "", o["kwh"] | 0.0f};
    if (b.name.length() && b.kwh > 0) p.batteryOptions.push_back(b);
  }
  for (JsonVariantConst c : s["init"].as<JsonArrayConst>()) p.init.push_back(c.as<String>());
  if (p.init.empty()) p.init = {"ATZ", "ATE0", "ATL0", "ATS0", "ATH0", "ATSP0"};
  for (JsonVariantConst j : s["pids"].as<JsonArrayConst>()) {
    PidDef d;
    d.id = j["id"] | "";
    d.id.toLowerCase();
    d.name = j["name"] | d.id.c_str();
    d.header = j["header"] | "";  d.header.toUpperCase();
    d.cmd = j["cmd"] | "";        d.cmd.toUpperCase();
    d.formula = j["formula"] | "";
    d.unit = j["unit"] | "";
    d.deviceClass = j["device_class"] | "";
    d.stateClass = j["state_class"] | "measurement";
    d.icon = j["icon"] | "";
    d.interval = max(5, j["interval"] | 120);
    if (j["min"].is<float>()) d.minVal = j["min"];
    if (j["max"].is<float>()) d.maxVal = j["max"];
    d.precision = constrain(j["precision"] | 1, 0, 4);
    d.enabled = j["enabled"] | true;
    d.sleepZero = j["sleep_zero"] | false;

    if (!validProfileName(d.id)) { err = String("Invalid ID: '") + d.id + "' (a-z, 0-9, _ -)"; return false; }
    if (d.header.length() && !isHex(d.header)) { err = d.id + ": header must be hex"; return false; }
    if (!isHex(d.cmd) || d.cmd.length() % 2) { err = d.id + ": command must be hex (e.g. 220105)"; return false; }
    if (d.formula.isEmpty()) { err = d.id + ": formula missing"; return false; }
    for (auto& o : p.pids) if (o.id == d.id) { err = String("Duplicate ID: ") + d.id; return false; }
    p.pids.push_back(d);
  }
  return true;
}

void profileToJson(const Profile& p, JsonDocument& d) {
  d["name"] = p.name;
  d["model"] = p.model;
  if (p.batteryKwh > 0) d["battery_kwh"] = p.batteryKwh;
  if (p.consumption > 0) d["consumption"] = p.consumption;
  if (!p.batteryOptions.empty()) {
    JsonArray bo = d["battery_options"].to<JsonArray>();
    for (auto& b : p.batteryOptions) { JsonObject o = bo.add<JsonObject>(); o["name"] = b.name; o["kwh"] = b.kwh; }
  }
  JsonArray in = d["init"].to<JsonArray>();
  for (auto& c : p.init) in.add(c);
  JsonArray arr = d["pids"].to<JsonArray>();
  for (auto& x : p.pids) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = x.id; o["name"] = x.name; o["header"] = x.header; o["cmd"] = x.cmd;
    o["formula"] = x.formula; o["unit"] = x.unit; o["device_class"] = x.deviceClass;
    o["state_class"] = x.stateClass; o["icon"] = x.icon; o["interval"] = x.interval;
    if (!isnan(x.minVal)) o["min"] = x.minVal;
    if (!isnan(x.maxVal)) o["max"] = x.maxVal;
    o["precision"] = x.precision; o["enabled"] = x.enabled;
    if (x.sleepZero) o["sleep_zero"] = true;
  }
}

String readProfileRaw(const String& name) {
  if (!validProfileName(name)) return "";
  File f = LittleFS.open("/profiles/" + name + ".json", "r");
  if (!f) return "";
  String s = f.readString();
  f.close();
  return s;
}

bool loadProfile(const String& name, Profile& out) {
  String raw = readProfileRaw(name);
  if (raw.isEmpty()) return false;
  JsonDocument d;
  if (deserializeJson(d, raw)) return false;
  String err;
  return parseProfile(d.as<JsonVariantConst>(), out, err);
}

bool saveProfileRaw(const String& name, const String& json, String& err) {
  if (!validProfileName(name)) { err = "Invalid profile name"; return false; }
  JsonDocument d;
  if (auto e = deserializeJson(d, json)) { err = String("JSON error: ") + e.c_str(); return false; }
  Profile tmp;
  if (!parseProfile(d.as<JsonVariantConst>(), tmp, err)) return false;
  JsonDocument clean;
  profileToJson(tmp, clean);
  File f = LittleFS.open("/profiles/" + name + ".json", "w");
  if (!f) { err = "Write error"; return false; }
  serializeJsonPretty(clean, f);
  f.close();
  return true;
}

std::vector<String> listProfiles() {
  std::vector<String> out;
  File dir = LittleFS.open("/profiles");
  if (!dir) return out;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    String n = f.name();
    int slash = n.lastIndexOf('/');
    if (slash >= 0) n = n.substring(slash + 1);
    if (n.endsWith(".json")) out.push_back(n.substring(0, n.length() - 5));
  }
  return out;
}
