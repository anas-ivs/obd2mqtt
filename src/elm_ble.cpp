#include "elm_ble.h"
#include <NimBLEDevice.h>
#include <mutex>
#include <map>
#include "config.h"
#include "log.h"
#include "watchdog.h"

namespace ElmBle {

volatile bool quiet = false;


static NimBLEClient* client = nullptr;
static NimBLERemoteCharacteristic* chrNotify = nullptr;
static NimBLERemoteCharacteristic* chrWrite = nullptr;
static bool writeWithResponse = false;
static String uuidInfo, devName, nameMac, kind;
static volatile uint32_t connectMs = 0;    // time of the last successful connect

static std::mutex rxMutex;
static String rxBuf;
static volatile bool rxPrompt = false;
static bool rxContent = false;          // real response characters received yet?
static volatile uint32_t rxLastMs = 0;
static volatile uint32_t strayPrompts = 0;
static volatile uint32_t drops = 0;        // unplanned disconnects (not 0x216)
static volatile uint8_t fastDrops = 0;     // 0x208 drops with short connection interval
static volatile bool relaxed = false;      // true = slower default connection parameters
static bool pairLogged = false;            // log "pairing OK" only the first time or after an error

static void onNotify(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
  std::lock_guard<std::mutex> lk(rxMutex);
  rxLastMs = millis();
  for (size_t i = 0; i < len; i++) {
    char c = (char)data[i];
    if (c == 0) continue;
    if (c == '>') {
      // A '>' before any response is a late prompt from the previous command
      // (e.g. double prompt after ATZ) – ignore it, otherwise the response gets truncated.
      if (!rxContent) { strayPrompts++; continue; }
      rxPrompt = true;
      continue;
    }
    if (rxPrompt) continue;              // append nothing after the prompt
    if (c != '\r' && c != '\n' && c != ' ') rxContent = true;
    if (rxBuf.length() < 4096) rxBuf += c;
  }
}

// Waits until the dongle is quiet (no more notifications for quietMs)
static void drain(uint32_t quietMs, uint32_t maxMs) {
  uint32_t t0 = millis();
  while (millis() - t0 < maxMs && millis() - rxLastMs < quietMs) delay(5);
}

class ClientCb : public NimBLEClientCallbacks {
  // Pairing with PIN (e.g. WiCAN Pro: passkey entry, encrypted connection)
  void onPassKeyEntry(NimBLEConnInfo& info) override {
    if (cfg.blePin.isEmpty()) {
      logf("%s", "Dongle requires a PIN – please enter it under Settings → BLE dongle");
      NimBLEDevice::injectPassKey(info, 0);
      return;
    }
    logf("%s", "Dongle requires PIN – sending stored PIN");
    NimBLEDevice::injectPassKey(info, (uint32_t)cfg.blePin.toInt());
  }
  void onConfirmPasskey(NimBLEConnInfo& info, uint32_t) override { NimBLEDevice::injectConfirmPasskey(info, true); }
  void onAuthenticationComplete(NimBLEConnInfo& info) override {
    if (info.isEncrypted()) {
      if (!pairLogged) logf("%s", "BLE pairing OK (encrypted)");
      pairLogged = true;
    } else {
      pairLogged = false;
      logf("%s", "BLE pairing failed – check the PIN (stored bond is discarded)");
      NimBLEDevice::deleteBond(info.getIdAddress());
    }
  }
  void onDisconnect(NimBLEClient*, int reason) override {
    if (reason == 0x216) {   // 0x216 = disconnected by us (planned)
      if (!quiet) logf("%s", "BLE disconnected (scheduled – dongle may sleep until the next poll)");
      return;
    }
    drops++;
    logf("BLE disconnected (reason 0x%X)", reason);
    // 0x208 = connection lost (supervision timeout). If this happens repeatedly with the short connection interval,
    // the dongle probably can't handle it → use default values (30–50 ms) from now on.
    // Only count drops shortly after connecting (driving the car away etc. should not trigger the switch)
    if (reason == 0x208 && !relaxed && millis() - connectMs < 60000 && ++fastDrops >= 2) {
      relaxed = true;
      logf("%s", "Connection dropped repeatedly (0x208) – using slower BLE connection parameters from now on");
    }
  }
};
static ClientCb clientCb;

void begin() {
  NimBLEDevice::init("obd2mqtt");
  NimBLEDevice::setMTU(247);   // larger packets: long responses (multi-frame) in fewer notifications
  NimBLEDevice::setPower(9);   // max. TX power (dBm), helps for garage → car
}

void shutdown() {
  disconnect();
  NimBLEDevice::deinit(true);
}

bool connected() { return client && client->isConnected(); }
uint32_t dropCount() { return drops; }
bool relaxedParams() { return relaxed; }
// RSSI right after connecting is often not measured yet (e.g. -128) → 0 = unknown
int rssi() {
  if (!connected()) return 0;
  int r = client->getRssi();
  return (r <= -120 || r >= 0) ? 0 : r;
}
uint16_t mtu() { return connected() ? client->getMTU() : 0; }
String detectedUuids() { return uuidInfo; }
String deviceName() { return devName.length() && nameMac == cfg.bleMac ? devName : cfg.bleName; }
String dongleKind() { return kind; }

void disconnect() {
  if (client) {
    if (client->isConnected()) client->disconnect();
    NimBLEDevice::deleteClient(client);
  }
  client = nullptr;
  chrNotify = chrWrite = nullptr;
}

static bool isStdService(const NimBLEUUID& u) {
  static const uint16_t std16[] = {0x1800, 0x1801, 0x180A, 0x180F, 0xFE59};
  for (uint16_t s : std16) if (u == NimBLEUUID(s)) return true;
  return false;
}

// Finds the notify and write characteristics. Order: manual UUIDs → known pairs → generic.
static bool resolveCharacteristics(String& err) {
  chrNotify = chrWrite = nullptr;

  if (cfg.bleService.length()) {
    NimBLERemoteService* s = client->getService(NimBLEUUID(cfg.bleService.c_str()));
    if (!s) { err = "Service " + cfg.bleService + " not found"; return false; }
    if (cfg.bleNotify.length()) chrNotify = s->getCharacteristic(NimBLEUUID(cfg.bleNotify.c_str()));
    if (cfg.bleWrite.length()) chrWrite = s->getCharacteristic(NimBLEUUID(cfg.bleWrite.c_str()));
    if (chrNotify && chrWrite) return true;
  }

  struct Known { const char *svc, *ntf, *wr; };
  static const Known known[] = {
    {"fff0", "fff1", "fff2"},   // many clones, Vgate iCar Pro and others
    {"ffe0", "ffe1", "ffe1"},   // HM-10-based dongles
    {"18f0", "2af0", "2af1"},   // Vgate / Viecar
    {"e7810a71-73ae-499d-8c15-faa9aef0c3f2", "bef8d6c9-9c21-4c9e-b632-bd58c1009f9f",
     "bef8d6c9-9c21-4c9e-b632-bd58c1009f9f"},   // vLinker / iOS-Vlink and others
  };
  for (auto& k : known) {
    Watchdog::feed();   // each service discovery can take up to 30 s
    if (!client->isConnected()) { err = "Connection dropped during setup"; return false; }
    NimBLERemoteService* s = client->getService(NimBLEUUID(k.svc));
    if (!s) continue;
    auto* n = s->getCharacteristic(NimBLEUUID(k.ntf));
    auto* w = s->getCharacteristic(NimBLEUUID(k.wr));
    if (n && w && (n->canNotify() || n->canIndicate()) && (w->canWrite() || w->canWriteNoResponse())) {
      chrNotify = n; chrWrite = w;
      return true;
    }
  }

  // Generic: first non-standard service with notify + write
  Watchdog::feed();
  if (!client->isConnected()) { err = "Connection dropped during setup"; return false; }
  for (NimBLERemoteService* s : client->getServices(true)) {
    Watchdog::feed();
    if (isStdService(s->getUUID())) continue;
    NimBLERemoteCharacteristic *n = nullptr, *w = nullptr;
    for (NimBLERemoteCharacteristic* c : s->getCharacteristics(true)) {
      if (!n && (c->canNotify() || c->canIndicate())) n = c;
      if (!w && (c->canWrite() || c->canWriteNoResponse())) w = c;
    }
    if (n && w) { chrNotify = n; chrWrite = w; return true; }
  }
  err = "No suitable notify/write characteristic found – enter UUIDs manually";
  return false;
}

bool connect(String& err) {
  if (connected()) return true;
  if (cfg.bleMac.length() != 17) { err = "No valid BLE MAC configured"; return false; }
  disconnect();

  // Security: with PIN → passkey entry (MITM, Secure Connections, bonding); without PIN → "Just Works"
  if (cfg.blePin.length()) {
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY);
  } else {
    NimBLEDevice::setSecurityAuth(true, false, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  }
  client = NimBLEDevice::createClient();
  client->setClientCallbacks(&clientCb, false);
  client->setConnectTimeout(6000);
  // Short connection interval (7.5–15 ms instead of the default 30–50 ms): more throughput so the buffer
  // of cheap dongles doesn't overflow on long responses (cause of "incomplete response").
  // Some dongles (e.g. Vgate) drop the connection with it → default values after repeated 0x208.
  if (relaxed) client->setConnectionParams(24, 40, 0, 600);
  else client->setConnectionParams(6, 12, 0, 400);

  std::vector<uint8_t> types;
  if (cfg.bleAddrType == "public") types = {BLE_ADDR_PUBLIC};
  else if (cfg.bleAddrType == "random") types = {BLE_ADDR_RANDOM};
  else types = {BLE_ADDR_PUBLIC, BLE_ADDR_RANDOM};

  // Each step blocks for up to 30 s (BLE timeout). To keep the 60 s watchdog from firing:
  // feed the watchdog before each step, remember the step for crash analysis, abort after 20 s.
  const uint32_t t0 = millis();
  auto step = [&](const char* name) -> bool {
    Watchdog::feed();
    Watchdog::step(name);
    if (millis() - t0 > 20000) {
      err = String("Connection setup takes too long (aborted at: ") + name + ")";
      return false;
    }
    if (strcmp(name, "BLE connect") && !client->isConnected()) {
      err = "Connection dropped during setup";
      return false;
    }
    return true;
  };
  auto fail = [&]() { Watchdog::step("BLE disconnect"); disconnect(); Watchdog::step("Poller"); return false; };

  bool ok = false;
  for (uint8_t t : types) {
    if (!step("BLE connect")) return fail();
    NimBLEAddress addr(std::string(cfg.bleMac.c_str()), t);
    if (client->connect(addr)) { ok = true; break; }
  }
  if (!ok) { err = "Dongle not reachable (car away / dongle asleep?)"; return fail(); }

  if (!step("BLE MTU")) return fail();
  client->exchangeMTU();
  if (!step("BLE services")) return fail();
  if (!resolveCharacteristics(err)) return fail();

  if (!step(cfg.blePin.length() ? "BLE subscribe/pairing" : "BLE subscribe")) return fail();
  bool sub = chrNotify->canNotify() ? chrNotify->subscribe(true, onNotify)
                                    : chrNotify->subscribe(false, onNotify);
  if (!sub) {
    if (err.isEmpty()) err = "Subscribe failed";
    return fail();
  }
  writeWithResponse = !chrWrite->canWriteNoResponse();

  uuidInfo = String(chrNotify->getRemoteService()->getUUID().toString().c_str()) + " / " +
             chrNotify->getUUID().toString().c_str() + " / " + chrWrite->getUUID().toString().c_str();
  // Read the device name from the GAP service (0x1800 / 0x2A00) – not every dongle provides it.
  // Only on the first connect: saves another (blocking) read request on every reconnect.
  if ((devName.isEmpty() || nameMac != cfg.bleMac) && step("BLE name")) {
    NimBLEAttValue v = client->getValue(NimBLEUUID((uint16_t)0x1800), NimBLEUUID((uint16_t)0x2A00));
    String n;
    for (size_t i = 0; i < v.length() && i < 40; i++) {
      char c = (char)v.data()[i];
      if (c >= 32 && c < 127) n += c;
    }
    n.trim();
    if (n.length()) { devName = n; nameMac = cfg.bleMac; }
  }
  // Rough type based on the service UUID
  {
    String svc = chrNotify->getRemoteService()->getUUID().toString().c_str();
    svc.toLowerCase();
    if (svc.indexOf("fff0") >= 0) kind = "ELM327 (FFF0)";
    else if (svc.indexOf("ffe0") >= 0) kind = "ELM327 / HM-10 (FFE0)";
    else if (svc.indexOf("18f0") >= 0) kind = "Vgate / Viecar (18F0)";
    else if (svc.indexOf("e7810a71") >= 0) kind = "vLinker / iOS-Vlink";
    else kind = "unknown type";
  }
  Watchdog::step("Poller");
  connectMs = millis();
  if (!client->isConnected()) { err = "Connection dropped during setup"; return fail(); }
  if (!quiet) {
    int r = rssi();
    String rs = r ? String(r) + " dBm" : String("not measured yet");
    logf("BLE connected: %s (%s), RSSI %s, MTU %u, UUIDs %s%s",
         deviceName().length() ? deviceName().c_str() : cfg.bleMac.c_str(), kind.c_str(), rs.c_str(),
         (unsigned)client->getMTU(), uuidInfo.c_str(),
         relaxed ? ", slow connection parameters" : "");
  }
  return true;
}

bool command(const String& cmd, String& response, uint32_t timeoutMs) {
  response = "";
  if (!connected() || !chrWrite) return false;
  drain(30, 300);                        // wait out leftovers of the previous response
  {
    std::lock_guard<std::mutex> lk(rxMutex);
    rxBuf = "";
    rxPrompt = false;
    rxContent = false;
  }
  String out = cmd + "\r";
  const uint8_t* p = (const uint8_t*)out.c_str();
  size_t left = out.length();
  while (left) {   // split into 20-byte chunks (default MTU)
    size_t n = min(left, (size_t)20);
    if (!chrWrite->writeValue(p, n, writeWithResponse)) return false;
    p += n; left -= n;
  }
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (rxPrompt) break;
    if (!connected()) return false;
    Watchdog::feed();
    delay(5);
  }
  std::lock_guard<std::mutex> lk(rxMutex);
  response = rxBuf;
  return rxPrompt;
}

// Remembers the last seen name per device: with the duplicate filter disabled, a later
// advertisement without a scan response would otherwise overwrite the name again.
static std::mutex scanMutex;
static std::map<std::string, std::string> scanNames;
class ScanCb : public NimBLEScanCallbacks {
  void remember(const NimBLEAdvertisedDevice* d) {
    std::string n = d->getName();
    if (n.empty()) return;
    std::lock_guard<std::mutex> lk(scanMutex);
    scanNames[d->getAddress().toString()] = n;
  }
  void onDiscovered(const NimBLEAdvertisedDevice* d) override { remember(d); }
  void onResult(const NimBLEAdvertisedDevice* d) override { remember(d); }
};
static ScanCb scanCb;

std::vector<ScanEntry> scan(uint32_t ms) {
  std::vector<ScanEntry> out;
  NimBLEScan* sc = NimBLEDevice::getScan();
  {
    std::lock_guard<std::mutex> lk(scanMutex);
    scanNames.clear();
  }
  sc->setScanCallbacks(&scanCb, true);
  sc->setActiveScan(true);
  sc->setInterval(100);
  sc->setWindow(99);
  // No duplicate filter: the controller remembers already seen devices even across multiple scans
  // and then discards the scan response – which is where many dongles put their name.
  // Result was: on the second search only the MAC without a name.
  sc->setDuplicateFilter(0);   // (setScanCallbacks(..., true) sets this too)
  NimBLEScanResults res = sc->getResults(ms, false);
  for (int i = 0; i < res.getCount(); i++) {
    const NimBLEAdvertisedDevice* d = res.getDevice(i);
    ScanEntry e;
    e.mac = d->getAddress().toString().c_str();
    e.mac.toUpperCase();
    e.name = d->getName().c_str();
    if (e.name.isEmpty()) {
      std::lock_guard<std::mutex> lk(scanMutex);
      auto it = scanNames.find(d->getAddress().toString());
      if (it != scanNames.end()) e.name = it->second.c_str();
    }
    // Name still empty (e.g. dongle sends it only rarely): use the known name for the configured dongle
    if (e.name.isEmpty() && e.mac.equalsIgnoreCase(cfg.bleMac)) e.name = deviceName();
    e.rssi = d->getRSSI();
    e.addrRandom = d->getAddress().getType() == BLE_ADDR_RANDOM;
    String n = e.name; n.toUpperCase();
    e.likelyObd = n.indexOf("OBD") >= 0 || n.indexOf("ELM") >= 0 || n.indexOf("VLINK") >= 0 ||
                  n.indexOf("VGATE") >= 0 || n.indexOf("IOS") >= 0 || n.indexOf("CX") >= 0 ||
                  n.indexOf("VEEPEAK") >= 0 || n.indexOf("KIWI") >= 0 ||
                  d->isAdvertisingService(NimBLEUUID((uint16_t)0xFFF0)) ||
                  d->isAdvertisingService(NimBLEUUID((uint16_t)0xFFE0)) ||
                  d->isAdvertisingService(NimBLEUUID((uint16_t)0x18F0));
    out.push_back(e);
  }
  sc->clearResults();
  std::sort(out.begin(), out.end(), [](const ScanEntry& a, const ScanEntry& b) {
    if (a.likelyObd != b.likelyObd) return a.likelyObd;
    return a.rssi > b.rssi;
  });
  return out;
}

}  // namespace ElmBle
