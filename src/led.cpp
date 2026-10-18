#include "led.h"
#include "config.h"

namespace Led {

static volatile bool wifi = false, ap = false, mqttDown = false, err = false, update = false;
static volatile bool fault = false, asleep = false;
static volatile uint8_t act = IDLE;
static volatile uint32_t lastReply = 0, testUntil = 0;
static volatile int testPin = -1;
static rmt_obj_t* rmt = nullptr;

void setNet(bool w, bool a, bool m, bool e) { wifi = w; ap = a; mqttDown = m; err = e; }
void setActivity(Activity a) { act = a; }
void reply() { lastReply = millis(); }
void setCars(bool f, bool s) { fault = f; asleep = s; }
void setUpdate(bool on) { update = on; }
void test(int pin) { testPin = pin; testUntil = millis() + 3000; }

bool pinOk(int pin) {
  if (pin < 0 || !GPIO_IS_VALID_OUTPUT_GPIO(pin)) return false;
#if CONFIG_IDF_TARGET_ESP32S3
  if (pin >= 26 && pin <= 37) return false;   // flash / PSRAM
#elif CONFIG_IDF_TARGET_ESP32C3
  if (pin >= 12 && pin <= 17) return false;   // flash
#elif CONFIG_IDF_TARGET_ESP32
  if (pin >= 6 && pin <= 11) return false;    // flash
#endif
  return true;
}

static void write(uint32_t rgb) {
  rmt_data_t d[24];
  uint8_t col[3] = {(uint8_t)(rgb >> 8), (uint8_t)(rgb >> 16), (uint8_t)rgb};   // WS2812 order: G, R, B
  int i = 0;
  for (uint8_t c : col)
    for (int b = 7; b >= 0; b--, i++) {
      bool one = c & (1 << b);
      d[i].level0 = 1; d[i].duration0 = one ? 8 : 4;   // 100 ns ticks
      d[i].level1 = 0; d[i].duration1 = one ? 4 : 8;
    }
  rmtWriteBlocking(rmt, d, 24);
}

// Colour 0xRRGGBB at brightness pct (0–100) × level (0–100)
static uint32_t scale(uint32_t rgb, int pct, int level = 100) {
  uint32_t k = pct * level;   // 0 … 10000
  uint32_t r = ((rgb >> 16) & 0xFF) * k / 10000, g = ((rgb >> 8) & 0xFF) * k / 10000, b = (rgb & 0xFF) * k / 10000;
  return r << 16 | g << 8 | b;
}

static const uint32_t WHITE = 0xFFFFFF, PURPLE = 0xA000FF, ORANGE = 0xFF5000, CYAN = 0x00FFFF, BLUE = 0x0000FF,
                      GREEN = 0x00FF00, YELLOW = 0xFFC000, RED = 0xFF0000;

static uint32_t frame(uint32_t now) {
  if (testUntil && (int32_t)(testUntil - now) > 0) {
    uint32_t left = testUntil - now;
    return scale(left > 2000 ? RED : left > 1000 ? GREEN : BLUE, 30);
  }
  int pct = cfg.ledBrightness;
  if (pct <= 0) return 0;
  auto blink = [&](uint32_t period) { return (now % period) < period / 2; };
  if (update) return blink(250) ? scale(CYAN, pct) : 0;
  if (err) return blink(250) ? scale(RED, pct) : 0;
  if (!wifi) return blink(2000) ? scale(ap ? PURPLE : WHITE, pct) : 0;
  if (mqttDown) return scale(ORANGE, pct);
  switch (act) {
    case CONNECTING: return blink(1000) ? scale(BLUE, pct) : 0;
    case POLLING:    return scale(GREEN, pct, now - lastReply < 80 ? 100 : 25);
    case CONNECTED:  return scale(GREEN, pct, 25);
    default: break;
  }
  if (fault) return scale(YELLOW, pct);
  if (asleep) return now % 5000 < 150 ? scale(BLUE, pct, 40) : 0;
  return scale(GREEN, pct, 25);
}

static void task(void*) {
  int cur = -1;
  uint32_t last = 0xFFFFFFFF;
  for (;;) {
    uint32_t now = millis();
    bool testing = testUntil && (int32_t)(testUntil - now) > 0;
    if (!testing) testUntil = 0;
    int pin = testing ? testPin : cfg.ledPin;
    if (!pinOk(pin)) pin = -1;
    if (pin != cur) {   // pin changed in the settings or for the test → move the RMT channel
      if (rmt) { write(0); rmtDeinit(rmt); rmt = nullptr; }
      cur = pin;
      if (pin >= 0 && (rmt = rmtInit(pin, RMT_TX_MODE, RMT_MEM_64)) != nullptr) rmtSetTick(rmt, 100);
      last = 0xFFFFFFFF;
    }
    uint32_t c = frame(now);
    if (rmt && c != last) { write(c); last = c; }
    vTaskDelay(pdMS_TO_TICKS(25));
  }
}

void begin() { xTaskCreate(task, "led", 2560, nullptr, 1, nullptr); }

}  // namespace Led
