#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>


// ---------- Device configuration (/config.json) ----------
struct AppConfig {
  // Network
  String wifiSsid, wifiPass;
  String hostname = "obd2mqtt";
  String webPass;                       // empty = no login (user: admin)
  String ipMode = "dhcp";               // dhcp | static (falls back to DHCP on failure)
  String ipAddr, ipGw, ipMask = "255.255.255.0", ipDns1, ipDns2;

  // MQTT
  String mqttHost;
  uint16_t mqttPort = 1883;
  String mqttUser, mqttPass;
  String mqttBase = "obd2mqtt";         // State topics: <base>/<pid>
  String haPrefix = "homeassistant";    // Discovery prefix
  String deviceName = "Auto OBD";       // Device name in Home Assistant

  // BLE dongle
  String bleMac;                        // AA:BB:CC:DD:EE:FF
  String bleAddrType = "auto";          // auto | public | random
  String bleService, bleNotify, bleWrite; // empty = auto-detect
  String bleName;                       // Display name (from scan or dongle)
  String blePin;                        // BLE passkey for dongles that require pairing (e.g. WiCAN Pro: 123456), empty = none

  // Polling
  String profile = "ioniq5";            // File /profiles/<name>.json
  bool pollEnabled = true;              // Master switch: automatic polling on/off
  float minVoltage = 12.2f;             // 12V protection, 0 = off
  float sleepVoltage = 13.2f;           // below this the car is considered asleep → no CAN queries (0 = off)
  uint32_t sleepPollMin = 0;            // poll anyway while the car sleeps: every N min (0 = never)
  float lowBattVoltage = 12.0f;         // 12V warning (HA sensor "12V low"), 0 = off
  uint32_t retrySec = 60;               // Pause after a failed BLE connection
  uint32_t cmdTimeoutMs = 3000;
  bool keepConnected = false;           // Keep BLE connected between polls
  uint32_t backoffSec = 600;            // Pause when the car does not respond (0 = off)

  // Time & log
  String ntpServer = "pool.ntp.org";
  String tz = "MYT-8";                  // POSIX time zone (Malaysia, UTC+8)
  bool updateCheck = true;              // check GitHub daily for new firmware
  bool logPersist = true;               // store log in flash (survives reboots)
};

// ---------- Vehicle profile (/profiles/<name>.json) ----------
struct PidDef {
  String id, name, header, cmd, formula;
  String unit, deviceClass, stateClass = "measurement", icon;
  uint32_t interval = 120;              // seconds
  float minVal = NAN, maxVal = NAN;     // plausibility filter
  int precision = 1;
  bool enabled = true;
  bool sleepZero = false;               // set to 0 when the car goes to sleep (e.g. charging power)
};

struct BatteryOption { String name; float kwh; };
struct Profile {
  String name, model;
  float batteryKwh = 0;                 // usable capacity (kWh) → formula variable CAP
  float consumption = 0;                // consumption (kWh/100 km) → formula variable CONS
  std::vector<BatteryOption> batteryOptions;   // selection list in the UI
  std::vector<String> init;
  std::vector<PidDef> pids;
};

extern AppConfig cfg;
extern bool g_ipFallback;          // static IP failed → DHCP active (main.cpp)
extern Profile profile;

bool fsBegin();
bool loadConfig();
bool saveConfig();
void migrateProfiles();              // repair stored profiles (e.g. missing ATFCSD)
void configToJson(JsonDocument& doc, bool maskSecrets);
void configFromJson(JsonVariantConst src);   // "********" = leave value unchanged

bool loadProfile(const String& name);
bool parseProfile(JsonVariantConst src, Profile& out, String& err);
void profileToJson(const Profile& p, JsonDocument& doc);
bool saveProfileRaw(const String& name, const String& json, String& err);
String readProfileRaw(const String& name);
std::vector<String> listProfiles();
void installDefaultProfiles(bool overwrite);
bool validProfileName(const String& n);

extern const char* MASK;
