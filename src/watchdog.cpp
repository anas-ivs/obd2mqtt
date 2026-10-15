#include "watchdog.h"
#include <esp_system.h>
#include <esp_task_wdt.h>
#include "log.h"

namespace Watchdog {

static const uint32_t MAGIC = 0x0BD2A5E1;
static const uint32_t TASK_WDT_S = 60;
static const uint32_t WIFI_MAX_MS = 15UL * 60 * 1000;   // WiFi down 15 min → restart
static const uint32_t MQTT_MAX_MS = 20UL * 60 * 1000;   // MQTT down 20 min → restart
static const uint32_t STABLE_MS = 3UL * 60 * 1000;      // stable for 3 min → clear crash counter
static const uint32_t SAFE_AFTER = 3;                   // from 3 consecutive crashes: safe mode

// survives software resets (but not power loss)
RTC_NOINIT_ATTR static uint32_t rtcMagic;
RTC_NOINIT_ATTR static uint32_t rtcCrashes;
RTC_NOINIT_ATTR static char rtcWhy[64];
RTC_NOINIT_ATTR static char rtcStep[32];
static String hang;

static bool safe = false;
static bool armed = false;
static bool stable = false;
static uint32_t wifiDownSince = 0, mqttDownSince = 0;
static String reason;
static esp_reset_reason_t rstCode = ESP_RST_UNKNOWN;
static String rstWhy;

static const char* rawReason(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:  return "Power on";
    case ESP_RST_SW:       return "Software restart";
    case ESP_RST_PANIC:    return "Crash (exception/panic)";
    case ESP_RST_INT_WDT:  return "Interrupt watchdog";
    case ESP_RST_TASK_WDT: return "Task watchdog (firmware hung)";
    case ESP_RST_WDT:      return "Watchdog";
    case ESP_RST_BROWNOUT: return "Brownout – check power supply/cable";
    case ESP_RST_DEEPSLEEP:return "Deep sleep";
    case ESP_RST_EXT:      return "Reset button";
    default:               return "unknown";
  }
}

void begin() {
  esp_reset_reason_t r = esp_reset_reason();
  if (rtcMagic != MAGIC || r == ESP_RST_POWERON || r == ESP_RST_BROWNOUT) {
    rtcMagic = MAGIC;
    rtcCrashes = 0;
    rtcWhy[0] = 0;
    rtcStep[0] = 0;
  }
  bool crash = r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT;
  if (crash) rtcCrashes++;

  rstCode = r;
  if (r == ESP_RST_SW && rtcWhy[0]) rstWhy = rtcWhy;
  rtcWhy[0] = 0;
  reason = resetReason();

  rtcStep[sizeof(rtcStep) - 1] = 0;
  if (crash && rtcStep[0]) hang = rtcStep;
  rtcStep[0] = 0;

  safe = rtcCrashes >= SAFE_AFTER;
  logf("Last restart: %s%s", reason.c_str(),
       crash ? (String(" (") + rtcCrashes + " in a row)").c_str() : "");
  if (hang.length()) logf("Firmware hung at: %s", hang.c_str());
  if (safe) logf("SAFE MODE: %lu crashes in a row – Bluetooth/polling disabled", (unsigned long)rtcCrashes);
}

void armTaskWdt() {
#if ESP_IDF_VERSION_MAJOR >= 5
  esp_task_wdt_config_t c = {.timeout_ms = TASK_WDT_S * 1000, .idle_core_mask = 0, .trigger_panic = true};
  esp_task_wdt_reconfigure(&c);
#else
  esp_task_wdt_init(TASK_WDT_S, true);
#endif
  if (esp_task_wdt_add(NULL) == ESP_OK) armed = true;
  logf("Watchdog active (hang > %lus → restart)", (unsigned long)TASK_WDT_S);
}

void feed() {
  if (armed) esp_task_wdt_reset();
}

void restart(const char* why) {
  strlcpy(rtcWhy, why, sizeof(rtcWhy));
  logf("Restart: %s", why);
  logFlush();
  delay(300);
  ESP.restart();
}

void clearCrashCounter() { rtcCrashes = 0; }

void step(const char* what) {
  if (!what) { rtcStep[0] = 0; return; }
  if (strncmp(rtcStep, what, sizeof(rtcStep) - 1) == 0) return;
  strlcpy(rtcStep, what, sizeof(rtcStep));
}
const char* hangStep() { return hang.c_str(); }

void loop(bool wifiUp, bool mqttConfigured, bool mqttUp, bool apClients) {
  feed();
  uint32_t now = millis();
  if (!stable && now > STABLE_MS) {
    stable = true;
    if (rtcCrashes) logf("%s", "System stable – crash counter reset");
    rtcCrashes = 0;
  }
  // WiFi down (and no one on the setup AP) → restart
  if (!wifiUp && !apClients) {
    if (!wifiDownSince) wifiDownSince = now;
    if (now - wifiDownSince > WIFI_MAX_MS) restart("WiFi not connected for 15 min");
  } else wifiDownSince = 0;
  // MQTT down although WiFi is up → restart
  if (wifiUp && mqttConfigured && !mqttUp) {
    if (!mqttDownSince) mqttDownSince = now;
    if (now - mqttDownSince > MQTT_MAX_MS) restart("MQTT not connected for 20 min");
  } else mqttDownSince = 0;
}

bool safeMode() { return safe; }
uint32_t crashCount() { return rtcCrashes; }
const char* resetReason() {
  static String s;
  s = rstWhy.length() ? String("Watchdog: ") + rstWhy : String(rawReason(rstCode));
  return s.c_str();
}

}  // namespace Watchdog
