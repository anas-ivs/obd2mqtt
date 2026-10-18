#pragma once
#include <Arduino.h>
#include <vector>
#include "config.h"

// BLE client for ELM327-compatible OBD2 dongles
namespace ElmBle {

struct ScanEntry { String mac, name; int rssi; bool addrRandom; bool likelyObd; };

void begin();
void setTxPower(int dbm);            // Bluetooth transmit power (dBm), clamped to what the chip supports
void shutdown();                    // turn Bluetooth off completely (frees RAM, e.g. for the online update)
bool connect(const CarConfig& car, String& err);   // that car's dongle (MAC/UUIDs/PIN); disconnects any other first
void disconnect();
extern volatile bool quiet;         // true = don't log connect/disconnect (car asleep, checked every minute)
bool connected();                   // any dongle connected
bool connectedTo(const CarConfig& car);   // this car's dongle connected
uint32_t dropCount();               // count of unplanned disconnects
bool relaxedParams(const CarConfig& car);
bool configured(const CarConfig& car);     // MAC (mac mode) or name (name mode) set
String currentAddress(const CarConfig& car); // MAC, or in name mode the last address found ("" = not found yet)               // true = switched to slow connection parameters after repeated 0x208
int  rssi();
uint16_t mtu();                     // negotiated MTU (0 = not connected)
String detectedUuids();             // "svc / notify / write" after a successful connection
String deviceName(const CarConfig& car);                // Bluetooth name of the dongle (GAP name or from the scan)
String dongleKind();                // rough type detection based on the UUIDs

// Sends a command and waits for the ELM prompt '>'. Returns: raw response (without '>')
bool command(const String& cmd, String& response, uint32_t timeoutMs);

std::vector<ScanEntry> scan(uint32_t ms);

}  // namespace ElmBle
