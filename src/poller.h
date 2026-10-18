#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <mutex>

// Polling logic: connect BLE, init, poll PIDs cyclically, publish values – for up to two cars
// (index 0/1), one BLE connection at a time.
// Runs exclusively in the Arduino loop() task; the web UI communicates via jobs/flags.
namespace Poller {

std::mutex& mutex();          // protects cfg/profiles/status between web and loop task

void begin(bool bleEnabled = true);   // false = safe mode (no BLE polling)
void loop();

// Calls from the web task
void requestConfigApply(const String& json);  // applies + saves configuration
void requestProfileReload();                  // reload the profiles of both cars
void requestPollNow(int car = -1);            // -1 = all cars; wakes a sleeping car for one full poll
void requestCheckNow(int car);                // check the 12 V now (no waiting); polls only if the car is awake
String lastErrorText(int car);                // current "last error" of a car (empty = none)
void requestEnabled(int car, bool on);        // polling switch of a car (web/MQTT) – saved in loop()
void setPaused(bool p, bool bleOff = false); // e.g. during OTA; bleOff: also switch the Bluetooth stack off
bool bleStopped();                            // Bluetooth stack off (only a restart turns it back on)
bool isPaused();

enum JobType { JOB_RAW, JOB_TEST, JOB_SCAN, JOB_DIAG };
void setDebug(uint32_t minutes);              // diagnostic log for N minutes (0 = off)
size_t diagChunk(size_t offset, uint8_t* buf, size_t maxLen);   // last diagnostic report (text)
bool submitJob(JobType t, int car, const String& header, const String& cmd, const String& formula,
               float cap = 0, float cons = 0);   // cap/cons: battery values for the test (0 = active profile)
void jobJson(JsonDocument& d);                // {state: idle|pending|running|done, result: {...}}

void statusJson(JsonDocument& d);

}  // namespace Poller
