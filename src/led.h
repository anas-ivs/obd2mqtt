#pragma once
#include <Arduino.h>

// Status LED (WS2812 RGB on most ESP32-S3/C3 dev boards). Runs in its own task, so it keeps
// blinking while the loop task is blocked in a BLE connect or command.
namespace Led {
enum Activity : uint8_t { IDLE, CONNECTING, POLLING, CONNECTED };
void begin();
void setNet(bool wifiUp, bool apMode, bool mqttDown, bool error);   // from the loop (main.cpp)
void setActivity(Activity a);
void reply();                       // one dongle reply received → short bright flash while polling
void setCars(bool fault, bool asleep);   // any awake car with an error / any car asleep
void setUpdate(bool on);            // firmware update running
void test(int pin);                 // red, green, blue for 1 s each on this pin (finding the LED's GPIO)
bool pinOk(int pin);                // usable for the LED (not a flash/PSRAM pin)
}
