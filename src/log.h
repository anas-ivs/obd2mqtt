#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Thread-safe log: ring buffer (RAM) + optionally persisted in flash (/log.txt, rotated to /log.old.txt)
void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logToJson(JsonArray arr, size_t maxLines = 60);
String logText();                 // entire RAM buffer as text
void logSetPersist(bool on);
bool logPersist();
void logFlush();                  // write pending lines to the file (from loop())
void logClear();
bool logReadFile(bool old, String& out);
size_t logReadChunk(bool old, size_t offset, uint8_t* buf, size_t maxLen);   // read in chunks (low RAM)
bool logFileExists(bool old);   // read log file (thread-safe)
void logBootCheck(uint32_t crashes);       // at boot: remove log files after a crash series/full flash
bool timeValid();                 // time set via NTP and plausible
void timeInit(const char* tz);    // early in setup(): time zone, plausibility check, NTP callback
void timeLoop(const char* server);  // in loop(): check NTP responses
