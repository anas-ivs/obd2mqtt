#pragma once
#include <Arduino.h>
#include <vector>

// BLE client for ELM327-compatible OBD2 dongles
namespace ElmBle {

struct ScanEntry { String mac, name; int rssi; bool addrRandom; bool likelyObd; };

void begin();
void shutdown();                    // turn Bluetooth off completely (frees RAM, e.g. for the online update)
bool connect(String& err);          // uses cfg.bleMac / UUIDs (or auto-detection)
void disconnect();
extern volatile bool quiet;         // true = don't log connect/disconnect (car asleep, checked every minute)
bool connected();
uint32_t dropCount();               // count of unplanned disconnects
bool relaxedParams();               // true = switched to slow connection parameters after repeated 0x208
int  rssi();
uint16_t mtu();                     // negotiated MTU (0 = not connected)
String detectedUuids();             // "svc / notify / write" after a successful connection
String deviceName();                // Bluetooth name of the dongle (GAP name or from the scan)
String dongleKind();                // rough type detection based on the UUIDs

// Sends a command and waits for the ELM prompt '>'. Returns: raw response (without '>')
bool command(const String& cmd, String& response, uint32_t timeoutMs);

std::vector<ScanEntry> scan(uint32_t ms);

}  // namespace ElmBle
