#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <mutex>

// Polling logic: connect BLE, init, poll PIDs cyclically, publish values.
// Runs exclusively in the Arduino loop() task; the web UI communicates via jobs/flags.
namespace Poller {

std::mutex& mutex();          // protects cfg/profile/status between web and loop task

void begin(bool bleEnabled = true);   // false = safe mode (no BLE polling)
void loop();

// Calls from the web task
void requestConfigApply(const String& json);  // applies + saves configuration
void requestProfileReload();                  // reload active profile
void requestPollNow();
String lastErrorText();                       // current "last error" (empty = none)
void requestEnabled(bool on);                 // main switch (web/MQTT) – saved in loop()
void setPaused(bool p);                       // e.g. during OTA
bool isPaused();

enum JobType { JOB_RAW, JOB_TEST, JOB_SCAN, JOB_DIAG };
void setDebug(uint32_t minutes);
size_t diagChunk(size_t offset, uint8_t* buf, size_t maxLen);   // last diagnostic report (text)              // diagnostic log for N minutes (0 = off)
bool submitJob(JobType t, const String& header, const String& cmd, const String& formula,
               float cap = 0, float cons = 0);   // cap/cons: battery values for the test (0 = active profile)
void jobJson(JsonDocument& d);                // {state: idle|pending|running|done, result: {...}}

void statusJson(JsonDocument& d);

}  // namespace Poller
