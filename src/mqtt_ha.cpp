#include "mqtt_ha.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "log.h"
#include "poller.h"
#include "online_update.h"

// Home Assistant layout:
//  - one "OBD2MQTT Bridge" device (the ESP32): firmware update; <bridge base>/status is the availability topic
//  - one device per enabled car with its values, 12 V, dongle, polling switch, last error and profile
namespace MqttHa {

static WiFiClient net;
static PubSubClient mqtt(net);
static uint32_t lastTry = 0;
static bool discoveryPending = true;
static std::vector<String> announced[NCARS];   // PID IDs already created via discovery, per car

String deviceId() {
  uint8_t m[6];
  WiFi.macAddress(m);
  char b[20];
  snprintf(b, sizeof(b), "obd2mqtt_%02x%02x%02x", m[3], m[4], m[5]);
  return b;
}
static String bridgeId() { return deviceId() + "_bridge"; }
static String carId(int car) { return deviceId() + "_car" + String(car + 1); }

static String bridgeTopic(const String& sub) { return cfg.mqttBase + "/" + sub; }
static String carTopic(int car, const String& sub) { return cfg.cars[car].mqttBase + "/" + sub; }

// car < 0 = bridge device
static void addDevice(JsonDocument& d, int car) {
  JsonObject dev = d["device"].to<JsonObject>();
  if (car < 0) {
    dev["identifiers"][0] = bridgeId();
    dev["name"] = "OBD2MQTT Bridge";
    dev["model"] = "OBD2 BLE Bridge";
  } else {
    const CarConfig& c = cfg.cars[car];
    dev["identifiers"][0] = carId(car);
    dev["name"] = c.deviceName.length() ? c.deviceName : String("Car ") + (car + 1);
    dev["model"] = profiles[car].model.length() ? profiles[car].model : String("Car");
    dev["via_device"] = bridgeId();
  }
  dev["manufacturer"] = "OBD2MQTT (ESP32)";
  dev["sw_version"] = FW_VERSION;
  dev["configuration_url"] = "http://" + WiFi.localIP().toString() + "/";
  JsonArray av = d["availability"].to<JsonArray>();
  av.add<JsonObject>()["topic"] = bridgeTopic("status");
}

// Empty document = remove the entity from HA
static void publishConfig(int car, const String& component, const String& objId, JsonDocument& d) {
  String uid = (car < 0 ? bridgeId() : carId(car)) + "_" + objId;
  String t = cfg.haPrefix + "/" + component + "/" + uid + "/config";
  String payload;
  if (!d.isNull()) {
    d["unique_id"] = uid;
    addDevice(d, car);
    serializeJson(d, payload);
  }
  mqtt.publish(t.c_str(), payload.c_str(), true);
}
static void removeConfig(int car, const char* component, const String& objId) {
  JsonDocument empty;
  publishConfig(car, component, objId, empty);
}

// <car base>/profile = display name, <car base>/profile/attributes = details (JSON)
void publishProfile(int car) {
  if (!mqtt.connected() || !cfg.cars[car].enabled) return;
  const Profile& p = profiles[car];
  const CarConfig& c = cfg.cars[car];
  mqtt.publish(carTopic(car, "profile").c_str(), (p.name.length() ? p.name : c.profile).c_str(), true);
  JsonDocument a;
  a["id"] = c.profile;
  a["name"] = p.name;
  a["model"] = p.model;
  if (p.batteryKwh > 0) a["battery_kwh"] = p.batteryKwh;
  if (p.consumption > 0) a["consumption_kwh_100km"] = p.consumption;
  JsonArray en = a["active_pids"].to<JsonArray>();
  for (auto& x : p.pids) if (x.enabled) en.add(x.id);
  a["firmware"] = FW_VERSION;
  String s;
  serializeJson(a, s);
  mqtt.publish(carTopic(car, "profile/attributes").c_str(), s.c_str(), true);
}

static const char* const FIXED[][2] = {   // fixed per-car entities: component, object id
  {"sensor", "voltage_12v"}, {"binary_sensor", "car"}, {"binary_sensor", "car_awake"},
  {"binary_sensor", "battery_low"}, {"switch", "polling"}, {"sensor", "ble_rssi"},
  {"sensor", "last_error"}, {"sensor", "profile"}, {"sensor", "last_data"}, {"sensor", "next_poll"}, {"button", "check_now"}};

static void removeCar(int car) {
  for (auto& id : announced[car]) removeConfig(car, "sensor", id);
  for (auto& p : profiles[car].pids) removeConfig(car, "sensor", p.id);
  for (auto& f : FIXED) removeConfig(car, f[0], f[1]);
  announced[car].clear();
}

static size_t sendCarDiscovery(int car) {
  const Profile& prof = profiles[car];
  mqtt.subscribe(carTopic(car, "polling/set").c_str());
  mqtt.subscribe(carTopic(car, "check/set").c_str());
  std::vector<String> now;
  for (auto& p : prof.pids) {
    JsonDocument d;
    if (p.enabled) {
      d["name"] = p.name;
      d["state_topic"] = carTopic(car, p.id);
      if (p.unit.length()) d["unit_of_measurement"] = p.unit;
      if (p.deviceClass.length()) d["device_class"] = p.deviceClass;
      if (p.stateClass.length()) d["state_class"] = p.stateClass;
      if (p.icon.length()) d["icon"] = p.icon;
      d["suggested_display_precision"] = p.precision;
      now.push_back(p.id);
    }
    publishConfig(car, "sensor", p.id, d);
  }
  // Delete PIDs that are no longer in the profile from HA
  for (auto& old : announced[car]) {
    bool still = false;
    for (auto& n : now) if (n == old) still = true;
    bool inProfile = false;
    for (auto& p : prof.pids) if (p.id == old) inProfile = true;
    if (!still && !inProfile) removeConfig(car, "sensor", old);
  }
  announced[car] = now;

  { // 12V system (measured by the dongle)
    JsonDocument d;
    d["name"] = "12V battery";
    d["state_topic"] = carTopic(car, "voltage_12v");
    d["unit_of_measurement"] = "V";
    d["device_class"] = "voltage";
    d["state_class"] = "measurement";
    d["suggested_display_precision"] = 1;
    publishConfig(car, "sensor", "voltage_12v", d);
  }
  { // Dongle reachable
    JsonDocument d;
    d["name"] = "OBD dongle connected";
    d["state_topic"] = carTopic(car, "car");
    d["payload_on"] = "online";
    d["payload_off"] = "offline";
    d["device_class"] = "connectivity";
    publishConfig(car, "binary_sensor", "car", d);
  }
  if (cfg.sleepVoltage > 0) { // Car awake (12V above threshold = DC/DC converter active)
    JsonDocument d;
    d["name"] = "Car awake";
    d["state_topic"] = carTopic(car, "car_awake");
    d["icon"] = "mdi:car-electric";
    publishConfig(car, "binary_sensor", "car_awake", d);
  } else removeConfig(car, "binary_sensor", "car_awake");
  if (cfg.lowBattVoltage > 0) { // 12V warning
    JsonDocument d;
    d["name"] = "12V low";
    d["state_topic"] = carTopic(car, "battery_low");
    d["device_class"] = "battery";   // ON = low
    publishConfig(car, "binary_sensor", "battery_low", d);
  } else removeConfig(car, "binary_sensor", "battery_low");
  { // Polling switch of this car
    JsonDocument d;
    d["name"] = "Polling";
    d["state_topic"] = carTopic(car, "polling");
    d["command_topic"] = carTopic(car, "polling/set");
    d["payload_on"] = "ON";
    d["payload_off"] = "OFF";
    d["icon"] = "mdi:car-connected";
    d["entity_category"] = "config";
    publishConfig(car, "switch", "polling", d);
  }
  mqtt.publish(carTopic(car, "polling").c_str(), cfg.cars[car].pollEnabled ? "ON" : "OFF", true);
  { // BLE signal
    JsonDocument d;
    d["name"] = "BLE Signal";
    d["state_topic"] = carTopic(car, "ble_rssi");
    d["unit_of_measurement"] = "dBm";
    d["device_class"] = "signal_strength";
    d["entity_category"] = "diagnostic";
    publishConfig(car, "sensor", "ble_rssi", d);
  }
  { // Poll now: 12 V check right away, polls all values only if the car is awake (never wakes a sleeping car).
    // Object id / topic stay "check_now" / "check/set" so existing entity ids and automations keep working.
    JsonDocument d;
    d["name"] = "Poll now";
    d["command_topic"] = carTopic(car, "check/set");
    d["payload_press"] = "PRESS";
    d["icon"] = "mdi:car-search";
    publishConfig(car, "button", "check_now", d);
  }
  { // Last data: time of the last successful poll (retained; not touched by bridge restarts)
    JsonDocument d;
    d["name"] = "Last data";
    d["state_topic"] = carTopic(car, "last_data");
    d["device_class"] = "timestamp";
    d["icon"] = "mdi:clock-check-outline";
    publishConfig(car, "sensor", "last_data", d);
  }
  { // Next poll: when the bridge serves this car next (connect, 12 V check or poll)
    JsonDocument d;
    d["name"] = "Next poll";
    d["state_topic"] = carTopic(car, "next_poll");
    d["device_class"] = "timestamp";
    d["icon"] = "mdi:timer-sand";
    publishConfig(car, "sensor", "next_poll", d);
  }
  { // Last error – publish current state immediately (overwrites old retained value)
    String le = Poller::lastErrorText(car);
    mqtt.publish(carTopic(car, "last_error").c_str(), le.length() ? le.c_str() : "none", true);
  }
  { // Last error
    JsonDocument d;
    d["name"] = "Last error";
    d["state_topic"] = carTopic(car, "last_error");
    d["entity_category"] = "diagnostic";
    d["icon"] = "mdi:alert-circle-outline";
    publishConfig(car, "sensor", "last_error", d);
  }
  { // Active vehicle profile (name as value, details as attributes)
    JsonDocument d;
    d["name"] = "Vehicle profile";
    d["state_topic"] = carTopic(car, "profile");
    d["json_attributes_topic"] = carTopic(car, "profile/attributes");
    d["entity_category"] = "diagnostic";
    d["icon"] = "mdi:car-info";
    publishConfig(car, "sensor", "profile", d);
  }
  publishProfile(car);
  return now.size();
}

static void sendDiscovery() {
  { // Firmware update (HA "update" entity: shows new version, Install starts the online update)
    JsonDocument d;
    d["name"] = "Firmware";
    d["state_topic"] = bridgeTopic("update");
    d["command_topic"] = bridgeTopic("update/install");
    d["payload_install"] = "install";
    d["device_class"] = "firmware";
    d["entity_category"] = "config";
    publishConfig(-1, "update", "firmware", d);
  }
  OnlineUpdate::publishMqtt();
  String info;
  for (int i = 0; i < NCARS; i++) {
    if (cfg.cars[i].enabled) {
      size_t n = sendCarDiscovery(i);
      info += String(info.length() ? ", " : "") + "car " + (i + 1) + ": " + n + " values";
    } else {
      removeCar(i);   // disabled: clear its retained discovery configs (also those from before a restart)
    }
  }
  logf("MQTT discovery sent (%s)", info.length() ? info.c_str() : "no car enabled");
}

static void onMessage(char* t, byte* payload, unsigned int len) {
  String tp(t), v;
  for (unsigned i = 0; i < len && i < 16; i++) v += (char)payload[i];
  v.trim();
  v.toUpperCase();
  if (tp == bridgeTopic("update/install")) {
    if (v == "INSTALL") {
      if (OnlineUpdate::requestInstall()) logf("%s", "Online update triggered via Home Assistant");
      else logf("%s", "Home Assistant: install ignored – no newer version known");
    }
    return;
  }
  for (int i = 0; i < NCARS; i++) {
    if (!cfg.cars[i].enabled) continue;
    if (tp == carTopic(i, "check/set")) { Poller::requestPollNow(i); continue; }
    if (tp != carTopic(i, "polling/set")) continue;
    bool on = v == "ON" || v == "1" || v == "TRUE";
    if (on || v == "OFF" || v == "0" || v == "FALSE") Poller::requestEnabled(i, on);
  }
}

void begin() {
  mqtt.setCallback(onMessage);
  mqtt.setBufferSize(1536);
  mqtt.setKeepAlive(60);   // a BLE GATT timeout blocks the loop for up to 30 s – the broker must not drop us
}

bool connected() { return mqtt.connected(); }
void requestDiscovery() { discoveryPending = true; }

void loop() {
  if (cfg.mqttHost.isEmpty() || WiFi.status() != WL_CONNECTED) return;
  if (!mqtt.connected()) {
    if (millis() - lastTry < 10000 && lastTry) return;
    lastTry = millis();
    mqtt.setServer(cfg.mqttHost.c_str(), cfg.mqttPort);
    String will = bridgeTopic("status");
    bool ok = mqtt.connect(deviceId().c_str(),
                           cfg.mqttUser.length() ? cfg.mqttUser.c_str() : nullptr,
                           cfg.mqttPass.length() ? cfg.mqttPass.c_str() : nullptr,
                           will.c_str(), 1, true, "offline");
    if (!ok) { logf("MQTT connection failed (rc=%d)", mqtt.state()); return; }
    logf("MQTT connected to %s:%u", cfg.mqttHost.c_str(), cfg.mqttPort);
    mqtt.publish(will.c_str(), "online", true);
    mqtt.subscribe(bridgeTopic("update/install").c_str());
    discoveryPending = true;   // also subscribes the cars' polling/set topics
  }
  if (discoveryPending) { discoveryPending = false; sendDiscovery(); }
  mqtt.loop();
}

void publishValue(int car, const String& id, double v, int precision) {
  if (!mqtt.connected()) return;
  mqtt.publish(carTopic(car, id).c_str(), String(v, precision).c_str(), true);
}

void publishText(int car, const String& sub, const String& v, bool retain) {
  if (!mqtt.connected()) return;
  mqtt.publish(carTopic(car, sub).c_str(), v.c_str(), retain);
}

void publishBridge(const String& sub, const String& v, bool retain) {
  if (!mqtt.connected()) return;
  mqtt.publish(bridgeTopic(sub).c_str(), v.c_str(), retain);
}

}  // namespace MqttHa
