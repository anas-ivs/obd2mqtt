#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

// Highest Bluetooth transmit power the chip supports (dBm) – classic ESP32 +9, ESP32-S3/C3 up to +20
#if CONFIG_IDF_TARGET_ESP32
#define BLE_TX_MAX 9
#else
#define BLE_TX_MAX 20
#endif

#ifndef FW_BOARD
#define FW_BOARD "esp32dev"   // build environment, part of the release file name
#endif


// ---------- Per-car settings (one dongle per car) ----------
static const int NCARS = 2;
struct CarConfig {
  bool enabled = false;                 // car 2 is optional
  String name;                          // display name in the web UI, e.g. "IONIQ 5"
  String deviceName;                    // device name in Home Assistant
  String mqttBase;                      // state topics: <base>/<pid>, e.g. obd2mqtt/car1
  // BLE dongle
  String bleMatch = "mac";              // find the dongle by: mac | name (for dongles whose MAC changes on every power-up)
  String bleMac;                        // AA:BB:CC:DD:EE:FF (name mode: last address picked in the scan)
  String bleAddrType = "auto";          // auto | public | random
  String bleService, bleNotify, bleWrite; // empty = auto-detect
  String bleName;                       // display name (from scan or dongle)
  String blePin;                        // BLE passkey for dongles that require pairing (e.g. WiCAN Pro: 123456), empty = none
  String profile;                       // file /profiles/<name>.json
  bool pollEnabled = true;              // automatic polling on/off for this car
};

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
  String mqttBase = "obd2mqtt";         // bridge topics (status, firmware update): <base>/...
  String haPrefix = "homeassistant";    // Discovery prefix

  CarConfig cars[NCARS];                // per-car settings, defaults in config.cpp

  // Polling (shared by both cars)
  float minVoltage = 12.2f;             // 12V protection, 0 = off
  float sleepVoltage = 13.2f;           // below this the car is considered asleep → no CAN queries (0 = off)
  uint32_t sleepPollMin = 0;            // poll anyway while the car sleeps: every N min (0 = never)
  uint32_t sleepCheckMin = 30;          // while asleep: check only the 12 V voltage every N min (doesn't wake the car)
  float lowBattVoltage = 12.0f;         // 12V warning (HA sensor "12V low"), 0 = off
  uint32_t retrySec = 60;               // Pause after a failed BLE connection
  uint32_t cmdTimeoutMs = 3000;
  bool keepConnected = false;           // Keep BLE connected between polls
  int bleTxPower = 9;                   // Bluetooth transmit power in dBm (classic ESP32: max 9, S3/C3: up to 20)
  uint32_t backoffSec = 600;            // Pause when the car does not respond (0 = off)

  // Status LED (WS2812 RGB): GPIO of the board's LED (-1 = none), brightness 0–100 % (0 = off)
#ifdef PIN_NEOPIXEL
  int ledPin = PIN_NEOPIXEL;            // ESP32-S3 DevKitC: 48 (some boards 38), ESP32-C3: 8
#else
  int ledPin = -1;                      // classic ESP32 boards have no RGB LED
#endif
  int ledBrightness = 20;

  // Time & log
  String ntpServer = "pool.ntp.org";
  String tz = "MYT-8";                  // POSIX time zone (Malaysia, UTC+8)
  bool updateCheck = true;              // check GitHub daily for new firmware
  bool logPersist = true;               // store log in flash (survives reboots)

  AppConfig();
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
extern Profile profiles[NCARS];    // active profile per car

bool fsBegin();
bool loadConfig();
bool saveConfig();
void configToJson(JsonDocument& doc, bool maskSecrets);
void configFromJson(JsonVariantConst src);   // "********" = leave value unchanged

bool loadProfile(const String& name, Profile& out);
bool parseProfile(JsonVariantConst src, Profile& out, String& err);
void profileToJson(const Profile& p, JsonDocument& doc);
bool saveProfileRaw(const String& name, const String& json, String& err);
String readProfileRaw(const String& name);
std::vector<String> listProfiles();
void installDefaultProfiles(bool overwrite);
bool validProfileName(const String& n);

extern const char* MASK;
