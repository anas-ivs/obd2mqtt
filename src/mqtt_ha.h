#pragma once
#include <Arduino.h>

namespace MqttHa {
void begin();
void loop();                       // maintain connection, discovery after (re)connect
bool connected();
String deviceId();
void requestDiscovery();           // e.g. after a profile change
void publishValue(const String& id, double v, int precision);
void publishText(const String& sub, const String& v, bool retain = true);
void publishProfile();             // <base>/profile + <base>/profile/attributes
}
