#pragma once
#include <Arduino.h>

// Watchdog & safe mode
//  - Hardware task watchdog: main loop hangs > 60 s → automatic restart
//  - Connection watchdog: WiFi or MQTT down for more than X min → restart
//  - Safe mode: after 3 consecutive crashes/watchdog resets the firmware starts
//    without Bluetooth/polling so the web UI stays reachable
namespace Watchdog {
void begin();                       // call early in setup()
void armTaskWdt();                  // at the end of setup()
void feed();                        // call in long wait loops
void loop(bool wifiUp, bool mqttConfigured, bool mqttUp, bool apClients);
bool safeMode();
uint32_t crashCount();
const char* resetReason();          // reason for the last restart (plain text)
void restart(const char* why);      // planned restart with reason
void clearCrashCounter();           // e.g. on manual restart
// Remembers (survives the restart) what the firmware is currently doing. If it hangs, the log after the
// watchdog restart shows where ("Firmware hung at: BLE subscribe/pairing"). nullptr = nothing special.
void step(const char* what);
const char* hangStep();             // step the firmware was stuck at before the last crash ("" = unknown)
}
