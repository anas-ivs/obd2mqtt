#pragma once
#include <Arduino.h>

namespace MqttHa {
void begin();
void loop();                       // maintain connection, discovery after (re)connect
bool connected();
String deviceId();
void requestDiscovery();           // e.g. after a profile or car settings change
// Per car (index 0/1): <car base>/<sub>
void publishValue(int car, const String& id, double v, int precision);
void publishText(int car, const String& sub, const String& v, bool retain = true);
void publishProfile(int car);      // <car base>/profile + <car base>/profile/attributes
// Bridge (the ESP32 itself): <bridge base>/<sub>, e.g. the firmware update state
void publishBridge(const String& sub, const String& v, bool retain = true);
}
