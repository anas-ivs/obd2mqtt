#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Online update via GitHub releases (github.com/anas-ivs/obd2mqtt):
// checks the latest release, shows its changes and installs the *-esp32-ota.bin on request.
// Runs in the main task (blocking, watchdog is fed) – web/MQTT only trigger it.
namespace OnlineUpdate {
void loop();                         // from loop(): run requests, daily check
void requestCheck();
bool requestInstall();               // false = no newer version known
void stateJson(JsonDocument& d);
void publishMqtt();                  // HA "update" entity: installed/latest version
}
