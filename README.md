# OBD2MQTT – BLE-OBD2 → MQTT → Home Assistant

> **Beta.** Tested on a Hyundai IONIQ 5 and a Kia EV6 (two cars, one bridge) with an ESP32-S3.
>
> Fork of **OBD2MQTT by [Frank Fuerniss (EIBHomeControl)](https://github.com/EIBHomeControl)**, who wrote the original firmware (v0.1 – v0.3.23). This fork (v0.4 and later) is maintained by [anas-ivs](https://github.com/anas-ivs) – see [Credits](#credits).

**[⬇ Download: latest firmware (Releases)](../../releases/latest)** · [Manual](docs/manual_en.html) · [Changelog](CHANGELOG.en.md)

ESP32 firmware that connects via Bluetooth LE to an ELM327-compatible OBD2 dongle in the car, polls configurable PIDs and publishes the values via MQTT (with Home Assistant auto-discovery). Everything is configured through a web interface – no YAML, no programming.

**Recommended setup:** the ESP32 stays in the garage (on WiFi, USB power supply), the dongle stays plugged into the car. As soon as the car is within BLE range (typically 5–15 m), values arrive in Home Assistant.

```
Car (OBD2 dongle) ──Bluetooth LE──► ESP32 in the garage ──WiFi──► MQTT ──► Home Assistant / evcc
```

## Features

- **Two cars**: one dongle per car, each with its own profile, MQTT base topic and Home Assistant device (one BLE connection at a time)
- Dongle found by MAC or by Bluetooth name (for dongles whose MAC changes on every power-up, e.g. vLinker MC+)
- BLE client for ELM327 dongles (auto-detects common UUIDs: FFF0, FFE0, 18F0, vLinker; can be overridden manually)
- Vehicle profiles as JSON (pre-installed: Hyundai IONIQ 5, Kia EV6), editable in the web UI
- Web interface and built-in manual (English), backup/restore of all settings, log with real time (NTP)
- PIDs with freely definable formulas (`B34/2`, `u16(B28,B29)/10`, `s16(..)`, `s8(..)`, `bit(x,n)`)
- **Test button** per PID: shows every response byte with its index so you can easily find byte positions
- ELM327 terminal in the browser
- MQTT auto-discovery: sensors appear automatically as a device in Home Assistant
- **Lets the car sleep:** below the sleep voltage only the 12 V is checked (every 30 min) – no CAN requests, so the car is never woken; "Poll now" doesn't wake it either
- **Smart poll intervals** (both cars): every 60 s while charging (AC/DC), 3 × every 5 min when awake, then every 30 min; dongle not found → next attempt after 30 min
- Car card shows when the next poll happens (with countdown); Home Assistant sensors "Last data" and "Next poll"
- 12 V protection (ATRV), BLE disconnects between polls so the dongle can sleep
- Setup access point with captive portal, mDNS (`http://obd2mqtt.local`), OTA updates, optional password protection
- **Online update** from the GitHub releases: daily check, changelog of the new version, install with one click (web interface or Home Assistant update entity)
- Watchdog with automatic restart and safe mode
- **Status LED** on ESP32-S3/C3 boards (RGB): blue = connecting, green = polling / idle, blue pulse = car asleep, yellow = dongle not found, orange = MQTT down, cyan = update, red = safe mode (Settings → Status LED, with a Test button to find the GPIO)
- **Diagnostic report** (one click, copy/download, no WiFi/IP/passwords) and a 2-hour diagnostic log – ideal for beta testers

## What you need

- **ESP32 board** with a USB power supply, within BLE range of the car:
  - **recommended: ESP32-S3** (e.g. N16R8 dev board, ideally with an external antenna) – more BLE range (up to +20 dBm), RGB status LED, USB-C
  - classic ESP32 (e.g. ESP32-DevKitC V4) also works (BLE up to +9 dBm, no RGB LED)
- **Bluetooth LE OBD2 dongle**, ELM327-compatible, e.g. vLinker MC+/FS, Veepeak BLE+, OBDLink CX, Vgate iCar Pro BLE or WiCAN Pro (BLE + protocol "elm327", PIN under Settings → Cars). *Classic Bluetooth (SPP) dongles without "LE" do not work.*
- Home Assistant with an MQTT broker (e.g. the Mosquitto add-on)

## Flashing

Each board has its own firmware files (`<board>` = `esp32dev` for the classic ESP32, `esp32s3` for ESP32-S3 boards):

- `obd2mqtt-vX.Y.Z-<board>-factory.bin` – complete image for the **first flash over USB**, at **address 0x0**
- `obd2mqtt-vX.Y.Z-<board>-ota.bin` – for later updates via web UI → System → Firmware update

The web upload refuses a file built for another chip.

**Option A – no toolchain:** flash the factory.bin at **address 0x0** with a web flasher in Chrome/Edge (e.g. https://espressif.github.io/esptool-js/). On ESP32-S3 boards with two USB-C ports use the one marked UART/COM; if the board is not detected, hold BOOT while plugging it in.

**Option B – PlatformIO:**
```bash
pio run -e esp32dev -t upload      # classic ESP32
pio run -e esp32s3 -t upload       # ESP32-S3
pio run -e esp32c3 -t upload       # ESP32-C3 (not tested)
pio device monitor
```

After `pio run` both files are copied to `firmware/` automatically. To move to a new board: on the old board download a backup (System → Backup & restore, incl. passwords), flash the new board, connect to its setup WiFi and restore the backup.

## First setup

1. After the first boot the ESP32 opens the WiFi network **`OBD2MQTT-xxxx`** (password `obd2mqtt`). Connect and open http://192.168.4.1.
2. **Settings → WiFi**: enter SSID and password, then **MQTT**: broker, user and password.
3. **Cars**: park the car with the dongle plugged in nearby, then click "Scan for dongles". Entries marked with ★ are most likely OBD adapters. Click "Use for …" for car 1. For a second car, enable car 2 and assign its own dongle the same way.
4. "Save & restart". Afterwards the interface is available at `http://obd2mqtt.local` or the assigned IP address.
5. **Profile**: select the matching profile and click "Use for …" the car. Pick the car next to the Test area and click "Test" next to `soc` to check that a value comes back.

## Profiles

All values of the factory profiles are enabled by default; you can switch individual values off in the profile.

### Hyundai IONIQ 5 (also Kia EV6 / IONIQ 6, E-GMP)

Init: `ATZ ATE0 ATL0 ATS0 ATH0 ATSP6 ATAT0 ATSTFF ATFCSH7E4 ATFCSD300000 ATFCSM1`. `ATAT0` (no adaptive timing) and the explicit flow control make long responses (`220101`, 62 bytes) reliable on cheap ELM327 clones – in a test with an "OBDII v1.5" dongle it went from 0 of 3 to 6 of 7 complete responses. The flow-control address follows each header change automatically (`7E4`, `7C6`, `7A0`).

| ID | Header | Command | Formula | Meaning |
|---|---|---|---|---|
| `soc` | 7E4 | 220105 | `B34/2` | displayed SoC |
| `bat_temp_max` / `bat_temp_min` | 7E4 | 220101 | `s8(B17)` / `s8(B18)` | battery temperature °C |
| `odometer` | 7C6 | 22B002 | `B9*65536+B10*256+B11` | odometer (instrument cluster, usually only with ignition on) |
| `range_calc` | 7E4 | 220105 | `B34/2*CAP/CONS` | **calculated** range |
| `soc_bms` | 7E4 | 220101 | `B7/2` | BMS SoC |
| `soh` | 7E4 | 220105 | `u16(B28,B29)/10` | state of health |
| `hv_current` / `hv_voltage` | 7E4 | 220101 | `s16(B13,B14)/10` / `u16(B15,B16)/10` | HV current (negative = charging) / HV voltage |
| `charge_power` | 7E4 | 220101 | `max(0,-s16(B13,B14)*u16(B15,B16)/100000)` | charging power kW |
| `charging` / `charging_dc` | 7E4 | 220101 | `lt(s16(B13,B14),-5)` / `gt(…kW…,11.5)` | charging (current < −0.5 A) / DC charging (> 11.5 kW) – confirmed on an IONIQ 5 (AC charging 3.7 kW: −7.0 A at 527.5 V). The charging bits in byte 12 always stay 0 on the IONIQ 5 |
| `tyre_p_fl/fr/rl/rr` | 7A0 | 22C00B | `B7`/`B12`/`B17`/`B22` `*0.2/14.5038` | tyre pressure bar (raw × 0.2 psi) – TPMS, probably only with ignition on |
| `tyre_t_fl/fr/rl/rr` | 7A0 | 22C00B | `B8`/`B13`/`B18`/`B23` `-50` | tyre temperature °C |

Sources: [evDash](https://github.com/nickn17/evDash), [WiCAN](https://github.com/meatpiHQ/wican-fw) (`vehicle_profiles/hyundai/ioniq5-6.json`), OVMS.

### Range

The car does not report the dashboard range via OBD – neither evDash nor WiCAN nor OVMS read it. `range_calc` therefore estimates it: SoC × usable capacity ÷ consumption. Under **Profile → Battery & range** you pick your battery variant from a list (known sizes pre-filled, value can be corrected by hand) and enter your average consumption in kWh/100 km. Formulas can use the variables `CAP` (usable kWh) and `CONS` (kWh/100 km). Pre-filled variants (usable capacity): IONIQ 5 58/63/72.6/77.4/84 kWh (≈54/60/70/74/80).

### Converting WiCAN profiles

WiCAN counts the bytes of the raw CAN frames including the ISO-TP control bytes; here only the payload is counted:

- Single-frame response: our `B` = WiCAN `B` − 1
- Multi-frame response: WiCAN `B2…B7` → our `B0…B5`; then WiCAN `B9…B15` → `B6…B12`, `B17…B23` → `B13…B19` etc. (every 8th byte is dropped)
- WiCAN `S21` (signed) → `s8(B17)`, `[B19:B20]` → `u16(..)`, `B15:7` (bit) → `bit(B12,7)`

Values flagged `"sleep_zero": true` in the profile JSON (e.g. charging power, charging) are set to 0 when the car falls asleep, so Home Assistant does not keep showing the last charging value.

### Adding your own PIDs

1. Profile → "+ PID", then enter header, command and a first formula (e.g. `B3`).
2. Click "Test". The bytes are shown with their index (B0, B1, …). `B0` is always the service byte of the response, i.e. 0x62 for mode 22 or 0x41 for mode 01.
3. Adjust the formula until the value is plausible. Min/max act as a plausibility filter so outliers are discarded.
4. Tick "Active" and click "Save profile". Home Assistant creates the sensor automatically.

## Watchdog & safe mode

- **Hang:** if the main loop doesn't respond for more than 60 s, the ESP32 restarts automatically.
- **Connectivity:** if WiFi is gone for 15 min or MQTT for 20 min, it restarts as well – unless someone is connected to the setup WiFi.
- **Safe mode:** after **3 crashes or watchdog resets in a row** the firmware starts without Bluetooth and polling. The web interface stays reachable and the status page shows a red notice. Once you have fixed the profile or settings, "Restart normally" returns to normal operation. The counter resets after 3 min of stable operation or a power cycle.
- *System* shows the reason for the last restart, e.g. "Task watchdog (firmware hung)" or "Brownout".

## MQTT

Each car has its own base topic (Settings → Cars, default `obd2mqtt/car1` and `obd2mqtt/car2`) and its own Home Assistant device. The bridge itself uses the bridge base topic (default `obd2mqtt`).

| Topic | Content |
|---|---|
| `<car base>/<pid-id>` | measured value (retained), e.g. `obd2mqtt/car1/soc` → `54.5` |
| `<car base>/voltage_12v` | 12 V voltage measured at the dongle |
| `<car base>/car` | `online` / `offline` – dongle reachable |
| `<car base>/polling` | `ON` / `OFF` – automatic polling of this car; switch it via `<car base>/polling/set` (in HA: switch "Polling") |
| `<car base>/battery_low` | `ON` / `OFF` – 12 V battery below the warning threshold (default 12.0 V) |
| `<car base>/car_awake` | `ON` / `OFF` – car awake (12 V above the sleep threshold, i.e. DC/DC converter active) |
| `<car base>/ble_rssi` | BLE signal strength |
| `<car base>/last_error` | last error |
| `<car base>/check/set` | `PRESS` → check the 12 V now; polls all values only if the car is awake, never wakes a sleeping car (HA button “Poll now”) |
| `<car base>/last_data` | time (UTC, ISO 8601) of the last poll that delivered values – HA timestamp sensor “Last data” |
| `<car base>/next_poll` | time (UTC, ISO 8601) the car is served next (poll, 12 V check or reconnect), `None` = polling off – HA timestamp sensor “Next poll” |
| `<car base>/profile` | name of the car's vehicle profile, e.g. `Hyundai IONIQ 5` (retained) |
| `<car base>/profile/attributes` | details as JSON: `{"id":"ioniq5","name":"Hyundai IONIQ 5","model":"…","active_pids":[…],"firmware":"…"}` |
| `<car base>/state` | all current values as **one** JSON after every poll cycle: `{"car":"IONIQ 5","profile":"Hyundai IONIQ 5","profile_id":"ioniq5","time":"…","values":{"soc":71.5,…},"voltage_12v":12.6}` |
| `obd2mqtt/status` | `online` / `offline` (last will of the bridge, availability of all entities) |
| `obd2mqtt/update` | firmware update state (HA update entity "Firmware" on the "OBD2MQTT Bridge" device) |

Discovery is published under `homeassistant/<component>/obd2mqtt_xxxxxx_car1_<id>/config` (car 2: `_car2_`, bridge: `_bridge_`).

**Upgrading from v0.4:** the existing dongle and profile settings become car 1. The topics change from `obd2mqtt/…` to `obd2mqtt/car1/…`, so Home Assistant creates new entities; delete the old device once in HA (Settings → Devices → MQTT).

## Notes

- **Let the car sleep:** every OBD request wakes the car. If the 12 V voltage is below the sleep threshold (default 13.2 V – car off, not charging), the bridge sends no requests and only checks the voltage every 30 min; as soon as the car wakes up by itself, normal polling resumes. "Poll now" never wakes a sleeping car. Adjustable under Settings → Polling.
- **Poll intervals** (Settings → Polling, same for both cars): charging every 60 s; awake but not charging 3 polls every 5 min, then every 30 min; dongle not found → next attempt after 30 min (after a failed setup with the dongle in range: 2 quick retries after 60 s). Values with a longer profile interval (odometer, SOH) keep their own.
- **12 V battery:** BLE is disconnected between polls and polling pauses below 12.2 V. If the car doesn't respond, the bridge waits 10 min. Use a dongle with auto-sleep (e.g. Vgate iCar Pro BLE, vLinker MC+/FS BLE, OBDLink CX).
- **When does the car respond?** On the IONIQ 5 the BMS usually only responds with ignition on or while charging. While asleep you get `NO DATA`. The last value is kept in Home Assistant (retained).
- Only **BLE** dongles (Bluetooth 4.0+) are supported, no Bluetooth Classic adapters (SPP).
- **Dongle quality matters:** cheap clones ("OBDII v1.5", MTU 23) often deliver long multi-frame responses truncated – the bridge retries, but values arrive later and the log shows retries. Good dongles (WiCAN Pro, vLinker, Vgate iCar 2S Pro, OBDLink CX; MTU 247) are noticeably more reliable. A BLE dongle accepts only **one** connection: close OBD apps on your phone (or switch off Bluetooth) while the bridge is running.
- **Connection drops (reason 0x208):** if the connection drops repeatedly right after connecting, the bridge switches to slower BLE connection parameters by itself (shown in the log and in the diagnostic report).
- **Watchdog restart:** after a hang the log now also shows *where* the firmware hung (e.g. "Firmware hung at: BLE subscribe/pairing").
- Commands expect `ATH0`/`ATS0`/`ATCAF1` (set in the init commands).

## Project structure

```
platformio.ini          build configuration (esp32dev / esp32s3 / esp32c3)
src/main.cpp            WiFi, setup AP, mDNS, main loop
src/elm_ble.*           BLE connection, UUID detection, ELM commands, scan
src/obd_parse.*         response parser (single/multi-frame) and formula evaluation
src/poller.*            poll scheduler, 12 V protection, jobs from the web UI
src/mqtt_ha.*           MQTT and Home Assistant discovery
src/led.*               status LED (WS2812 via RMT, own task)
src/web.*, web_ui.h     REST API and web interface
scripts/gen_web_assets.py  gzips web_ui.h / help_ui.h into src/web_assets.h before each build
src/help_ui.h           built-in manual
src/config.*            configuration and profiles (LittleFS)
src/default_profiles.h  factory profiles IONIQ 5, EV6
lib/tinyexpr            formula parser (zlib license, codeplea/tinyexpr)
changelog.json          changelog source → tools/gen_changelog.py → CHANGELOG.en.md + web UI
```

## Disclaimer

Private hobby project, beta software, use **at your own risk**. No warranty or liability for damage to the vehicle, 12 V battery, dongle or other hardware, to the extent permitted by law. The factory profiles only send diagnostic **read** requests (UDS 0x22 / OBD2 mode 01); the terminal, however, can send arbitrary commands – at your own responsibility. Values without guarantee; the display in the vehicle is always authoritative. A permanently plugged-in dongle draws power; unplug it if the car is parked for a longer time.

Not affiliated with Hyundai, Kia, MeatPi or any dongle manufacturer. Brand and product names belong to their respective owners and are used for descriptive purposes only.

This firmware was developed with AI assistance.

## Credits

- **Original author: Frank Fuerniss ([EIBHomeControl](https://github.com/EIBHomeControl))** – created OBD2MQTT: BLE ELM327 client, profiles and formula engine, web interface, MQTT / Home Assistant discovery, sleep mode, watchdog & safe mode, online update (v0.1 – v0.3.23). Thank you for the excellent base and for releasing it under the MIT license.
- **This fork: [anas-ivs](https://github.com/anas-ivs)** – v0.4 and later: English only, two cars, find dongle by name, tyre values, ESP32-S3 support, new phone-friendly web UI, status LED, smart poll intervals, "Next poll". Developed with AI assistance (Claude Code).
- **PID research and vehicle data:** [evDash](https://github.com/nickn17/evDash) (nickn17), [WiCAN](https://github.com/meatpiHQ/wican-fw) (MeatPi – vehicle profiles), OVMS (Open Vehicle Monitoring System), and the community on the ABRP feedback board and in the forums.
- **Libraries:** [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) (h2zero), [ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer) / [AsyncTCP](https://github.com/ESP32Async/AsyncTCP) (ESP32Async), [ArduinoJson](https://github.com/bblanchon/ArduinoJson) (bblanchon), [PubSubClient](https://github.com/knolleary/pubsubclient) (knolleary), [tinyexpr](https://github.com/codeplea/tinyexpr) (codeplea), Arduino-ESP32 / ESP-IDF (Espressif).

## License

[MIT](LICENSE) – original copyright © 2026 Frank Fuerniss; the MIT license and its copyright notice apply to this fork as well. Bundled/used libraries are subject to their own licenses: NimBLE-Arduino (Apache 2.0), ESPAsyncWebServer / AsyncTCP (LGPL 3.0), ArduinoJson (MIT), PubSubClient (MIT), tinyexpr (zlib, included in `lib/tinyexpr`).
