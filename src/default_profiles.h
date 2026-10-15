#pragma once
// Factory profiles – written to /profiles/ on first boot
// and editable in the web UI afterwards.
//
// Formulas: B0..Bn = bytes of the (reassembled) response INCLUDING service bytes.
//   Example 220105 → response "62 01 05 ..." → B0=0x62, B1=0x01, B2=0x05, B3=first data byte
// Helper functions: u16(hi,lo), s16(hi,lo), s8(x), bit(x,n)
//
// Converting WiCAN profiles (github.com/meatpiHQ/wican-fw), which count raw CAN frames:
//   Single frame:  our B = WiCAN B - 1
//   Multi frame:   WiCAN B2..B7 → our B0..B5;  from WiCAN B9: our B = 6 + 7*(k-1) + (W - 8k - 1),
//                  k = frame no. (W div 8). Example: WiCAN B41 → our B34.

// Hyundai IONIQ 5 / Kia EV6 / IONIQ 6 (E-GMP)
// Offsets from evDash (CarHyundaiEgmp.cpp), cross-checked against WiCAN ioniq5-6.json and OVMS
static const char PROFILE_IONIQ5[] PROGMEM = R"JSON({
  "name": "Hyundai IONIQ 5 / Kia EV6",
  "model": "Hyundai IONIQ 5 (E-GMP)",
  "battery_kwh": 74, "consumption": 19,
  "battery_options": [
    { "name": "58 kWh (usable approx. 54 kWh)", "kwh": 54 },
    { "name": "63 kWh, from 2024 (usable approx. 60 kWh)", "kwh": 60 },
    { "name": "72.6 kWh (usable approx. 70 kWh)", "kwh": 70 },
    { "name": "77.4 kWh (usable approx. 74 kWh)", "kwh": 74 },
    { "name": "84 kWh, from 2024 (usable approx. 80 kWh)", "kwh": 80 }
  ],
  "init": ["ATZ", "ATE0", "ATL0", "ATS0", "ATH0", "ATSP6", "ATAT0", "ATSTFF", "ATFCSH7E4", "ATFCSD300000", "ATFCSM1"],
  "pids": [
    { "id": "soc", "name": "SoC", "header": "7E4", "cmd": "220105",
      "formula": "B34/2", "unit": "%", "device_class": "battery",
      "interval": 120, "min": 0, "max": 100, "precision": 1, "enabled": true },
    { "id": "bat_temp_max", "name": "Battery temp max", "header": "7E4", "cmd": "220101",
      "formula": "s8(B17)", "unit": "°C", "device_class": "temperature",
      "interval": 300, "min": -40, "max": 90, "precision": 0, "enabled": true },
    { "id": "bat_temp_min", "name": "Battery temp min", "header": "7E4", "cmd": "220101",
      "formula": "s8(B18)", "unit": "°C", "device_class": "temperature",
      "interval": 300, "min": -40, "max": 90, "precision": 0, "enabled": true },
    { "id": "odometer", "name": "Odometer", "header": "7C6", "cmd": "22B002",
      "formula": "B9*65536+B10*256+B11", "unit": "km", "device_class": "distance",
      "state_class": "total_increasing", "icon": "mdi:counter",
      "interval": 1800, "min": 1, "max": 2000000, "precision": 0, "enabled": true },
    { "id": "range_calc", "name": "Range (calculated)", "header": "7E4", "cmd": "220105",
      "formula": "B34/2*CAP/CONS", "unit": "km", "device_class": "distance",
      "icon": "mdi:map-marker-distance",
      "interval": 120, "min": 0, "max": 800, "precision": 0, "enabled": true },
    { "id": "soc_bms", "name": "SoC (BMS raw)", "header": "7E4", "cmd": "220101",
      "formula": "B7/2", "unit": "%", "device_class": "battery",
      "interval": 120, "min": 0, "max": 100, "precision": 1, "enabled": true },
    { "id": "soh", "name": "SoH", "header": "7E4", "cmd": "220105",
      "formula": "u16(B28,B29)/10", "unit": "%", "icon": "mdi:battery-heart-variant",
      "interval": 3600, "min": 0, "max": 100, "precision": 1, "enabled": true },
    { "id": "hv_current", "name": "HV current (negative = charging)", "header": "7E4", "cmd": "220101",
      "formula": "s16(B13,B14)/10", "unit": "A", "device_class": "current",
      "interval": 120, "min": -1000, "max": 1000, "precision": 1, "enabled": true, "sleep_zero": true },
    { "id": "hv_voltage", "name": "HV voltage", "header": "7E4", "cmd": "220101",
      "formula": "u16(B15,B16)/10", "unit": "V", "device_class": "voltage",
      "interval": 120, "min": 100, "max": 1000, "precision": 1, "enabled": true },
    { "id": "charge_power", "name": "Charging power", "header": "7E4", "cmd": "220101",
      "formula": "max(0,-s16(B13,B14)*u16(B15,B16)/100000)", "unit": "kW", "device_class": "power",
      "interval": 120, "min": 0, "max": 400, "precision": 1, "enabled": true, "sleep_zero": true },
    { "id": "charging", "name": "Charging", "header": "7E4", "cmd": "220101",
      "formula": "lt(s16(B13,B14),-5)", "icon": "mdi:ev-station", "state_class": "",
      "interval": 120, "min": 0, "max": 1, "precision": 0, "enabled": true, "sleep_zero": true },
    { "id": "charging_dc", "name": "DC charging", "header": "7E4", "cmd": "220101",
      "formula": "gt(-s16(B13,B14)*u16(B15,B16)/100000,11.5)", "icon": "mdi:ev-plug-ccs2", "state_class": "",
      "interval": 120, "min": 0, "max": 1, "precision": 0, "enabled": true, "sleep_zero": true }
  ]
})JSON";

// XPeng G9 (also G6/P5/P7/X9) – BMS via header 704, response from 784.
// Sources: WiCAN xpeng_g6.json, corrected per XPCarData (github.com/stevelea/xpcardata, verified on the G6):
//   SOH = 22110A (not 22011A as in WiCAN), odometer = 3 bytes, range = 221118 (CLTC).
// Not yet verified on the G9 (esp. MY25) – check every PID with "Test" against the car's display.
static const char PROFILE_G9[] PROGMEM = R"JSON({
  "name": "XPeng G9",
  "model": "XPeng G9",
  "battery_kwh": 92.2, "consumption": 20,
  "battery_options": [
    { "name": "78.2 kWh – Standard Range (MY25)", "kwh": 78.2 },
    { "name": "92.2 kWh – Long Range / Performance (MY25)", "kwh": 92.2 },
    { "name": "98 kWh – 2023/2024 (usable approx. 93 kWh)", "kwh": 93 }
  ],
  "init": ["ATZ", "ATE0", "ATL0", "ATS0", "ATH0", "ATSP6", "ATAT1", "ATAL", "ATCRA784", "ATFCSH704", "ATFCSD300000", "ATFCSM1"],
  "pids": [
    { "id": "soc", "name": "SoC", "header": "704", "cmd": "221109",
      "formula": "u16(B3,B4)/10", "unit": "%", "device_class": "battery",
      "interval": 120, "min": 0, "max": 100, "precision": 1, "enabled": true },
    { "id": "bat_temp_max", "name": "Battery temp max", "header": "704", "cmd": "221107",
      "formula": "B3-40", "unit": "°C", "device_class": "temperature",
      "interval": 300, "min": -40, "max": 90, "precision": 0, "enabled": true },
    { "id": "bat_temp_min", "name": "Battery temp min", "header": "704", "cmd": "221108",
      "formula": "B3-40", "unit": "°C", "device_class": "temperature",
      "interval": 300, "min": -40, "max": 90, "precision": 0, "enabled": true },
    { "id": "odometer", "name": "Odometer", "header": "704", "cmd": "220101",
      "formula": "B3*65536+B4*256+B5", "unit": "km", "device_class": "distance",
      "state_class": "total_increasing", "icon": "mdi:counter",
      "interval": 1800, "min": 1, "max": 2000000, "precision": 0, "enabled": true },
    { "id": "range_cltc", "name": "Range (CLTC)", "header": "704", "cmd": "221118",
      "formula": "u16(B3,B4)", "unit": "km", "device_class": "distance",
      "icon": "mdi:map-marker-distance",
      "interval": 300, "min": 0, "max": 1000, "precision": 0, "enabled": true },
    { "id": "range_calc", "name": "Range (calculated)", "header": "704", "cmd": "221109",
      "formula": "u16(B3,B4)/10*CAP/CONS", "unit": "km", "device_class": "distance",
      "icon": "mdi:map-marker-distance",
      "interval": 120, "min": 0, "max": 800, "precision": 0, "enabled": true },
    { "id": "soh", "name": "SoH", "header": "704", "cmd": "22110A",
      "formula": "u16(B3,B4)/10", "unit": "%", "icon": "mdi:battery-heart-variant",
      "interval": 3600, "min": 0, "max": 100, "precision": 1, "enabled": true },
    { "id": "charge_limit", "name": "Charge limit", "header": "704", "cmd": "221130",
      "formula": "u16(B3,B4)-10", "unit": "%", "icon": "mdi:battery-charging-high",
      "interval": 900, "min": 0, "max": 100, "precision": 0, "enabled": true },
    { "id": "charge_status", "name": "Charging status (raw, 0 = not charging)", "header": "704", "cmd": "22112D",
      "formula": "B3", "state_class": "", "icon": "mdi:ev-station",
      "interval": 120, "min": 0, "max": 10, "precision": 0, "enabled": true, "sleep_zero": true },
    { "id": "hv_voltage", "name": "HV voltage", "header": "704", "cmd": "221101",
      "formula": "u16(B3,B4)/10", "unit": "V", "device_class": "voltage",
      "interval": 120, "min": 0, "max": 1000, "precision": 1, "enabled": true },
    { "id": "hv_current", "name": "HV current (negative = charging)", "header": "704", "cmd": "221103",
      "formula": "u16(B3,B4)*0.5-1600", "unit": "A", "device_class": "current",
      "interval": 120, "min": -1600, "max": 1600, "precision": 1, "enabled": true, "sleep_zero": true },
    { "id": "charge_power", "name": "Charging power", "header": "704", "cmd": "221103",
      "formula": "max(0,-(u16(B3,B4)*0.5-1600)*hv_voltage/1000)", "unit": "kW", "device_class": "power",
      "interval": 120, "min": 0, "max": 500, "precision": 1, "enabled": true, "sleep_zero": true },
    { "id": "charging", "name": "Charging", "header": "704", "cmd": "221103",
      "formula": "lt(u16(B3,B4)*0.5-1600,-2)", "icon": "mdi:ev-station", "state_class": "",
      "interval": 120, "min": 0, "max": 1, "precision": 0, "enabled": true, "sleep_zero": true },
    { "id": "charging_dc", "name": "DC charging", "header": "704", "cmd": "221103",
      "formula": "gt(-(u16(B3,B4)*0.5-1600)*hv_voltage/1000,11.5)", "icon": "mdi:ev-plug-ccs2", "state_class": "",
      "interval": 120, "min": 0, "max": 1, "precision": 0, "enabled": true, "sleep_zero": true }
  ]
})JSON";
// XPeng G6 – same BMS PIDs as the G9 (704/784). The XPCarData values are verified on the G6.
// Range (calculated): adjust capacity/consumption to your variant (SR approx. 66 kWh, LR approx. 87.5 kWh gross).
static const char PROFILE_G6[] PROGMEM = R"JSON({
  "name": "XPeng G6",
  "model": "XPeng G6",
  "battery_kwh": 80, "consumption": 19,
  "battery_options": [
    { "name": "68.5 kWh – Standard Range (MY25)", "kwh": 67.8 },
    { "name": "80.8 kWh – Long Range / AWD (MY25)", "kwh": 80 },
    { "name": "66 kWh – 2024 (usable approx. 64 kWh)", "kwh": 64 },
    { "name": "87.5 kWh – 2024 (usable approx. 84 kWh)", "kwh": 84 }
  ],
  "init": ["ATZ", "ATE0", "ATL0", "ATS0", "ATH0", "ATSP6", "ATAT1", "ATAL", "ATCRA784", "ATFCSH704", "ATFCSD300000", "ATFCSM1"],
  "pids": [
    { "id": "soc", "name": "SoC", "header": "704", "cmd": "221109",
      "formula": "u16(B3,B4)/10", "unit": "%", "device_class": "battery",
      "interval": 120, "min": 0, "max": 100, "precision": 1, "enabled": true },
    { "id": "bat_temp_max", "name": "Battery temp max", "header": "704", "cmd": "221107",
      "formula": "B3-40", "unit": "°C", "device_class": "temperature",
      "interval": 300, "min": -40, "max": 90, "precision": 0, "enabled": true },
    { "id": "bat_temp_min", "name": "Battery temp min", "header": "704", "cmd": "221108",
      "formula": "B3-40", "unit": "°C", "device_class": "temperature",
      "interval": 300, "min": -40, "max": 90, "precision": 0, "enabled": true },
    { "id": "odometer", "name": "Odometer", "header": "704", "cmd": "220101",
      "formula": "B3*65536+B4*256+B5", "unit": "km", "device_class": "distance",
      "state_class": "total_increasing", "icon": "mdi:counter",
      "interval": 1800, "min": 1, "max": 2000000, "precision": 0, "enabled": true },
    { "id": "range_cltc", "name": "Range (CLTC)", "header": "704", "cmd": "221118",
      "formula": "u16(B3,B4)", "unit": "km", "device_class": "distance",
      "icon": "mdi:map-marker-distance",
      "interval": 300, "min": 0, "max": 1000, "precision": 0, "enabled": true },
    { "id": "range_calc", "name": "Range (calculated)", "header": "704", "cmd": "221109",
      "formula": "u16(B3,B4)/10*CAP/CONS", "unit": "km", "device_class": "distance",
      "icon": "mdi:map-marker-distance",
      "interval": 120, "min": 0, "max": 800, "precision": 0, "enabled": true },
    { "id": "soh", "name": "SoH", "header": "704", "cmd": "22110A",
      "formula": "u16(B3,B4)/10", "unit": "%", "icon": "mdi:battery-heart-variant",
      "interval": 3600, "min": 0, "max": 100, "precision": 1, "enabled": true },
    { "id": "charge_limit", "name": "Charge limit", "header": "704", "cmd": "221130",
      "formula": "u16(B3,B4)-10", "unit": "%", "icon": "mdi:battery-charging-high",
      "interval": 900, "min": 0, "max": 100, "precision": 0, "enabled": true },
    { "id": "charge_status", "name": "Charging status (raw, 0 = not charging)", "header": "704", "cmd": "22112D",
      "formula": "B3", "state_class": "", "icon": "mdi:ev-station",
      "interval": 120, "min": 0, "max": 10, "precision": 0, "enabled": true, "sleep_zero": true },
    { "id": "hv_voltage", "name": "HV voltage", "header": "704", "cmd": "221101",
      "formula": "u16(B3,B4)/10", "unit": "V", "device_class": "voltage",
      "interval": 120, "min": 0, "max": 1000, "precision": 1, "enabled": true },
    { "id": "hv_current", "name": "HV current (negative = charging)", "header": "704", "cmd": "221103",
      "formula": "u16(B3,B4)*0.5-1600", "unit": "A", "device_class": "current",
      "interval": 120, "min": -1600, "max": 1600, "precision": 1, "enabled": true, "sleep_zero": true },
    { "id": "charge_power", "name": "Charging power", "header": "704", "cmd": "221103",
      "formula": "max(0,-(u16(B3,B4)*0.5-1600)*hv_voltage/1000)", "unit": "kW", "device_class": "power",
      "interval": 120, "min": 0, "max": 500, "precision": 1, "enabled": true, "sleep_zero": true },
    { "id": "charging", "name": "Charging", "header": "704", "cmd": "221103",
      "formula": "lt(u16(B3,B4)*0.5-1600,-2)", "icon": "mdi:ev-station", "state_class": "",
      "interval": 120, "min": 0, "max": 1, "precision": 0, "enabled": true, "sleep_zero": true },
    { "id": "charging_dc", "name": "DC charging", "header": "704", "cmd": "221103",
      "formula": "gt(-(u16(B3,B4)*0.5-1600)*hv_voltage/1000,11.5)", "icon": "mdi:ev-plug-ccs2", "state_class": "",
      "interval": 120, "min": 0, "max": 1, "precision": 0, "enabled": true, "sleep_zero": true }
  ]
})JSON";

// XPeng P7+ – EXPERIMENTAL: no published PIDs; assumes the same BMS addressing (704/784)
// as the G6/G9. Please check every value with "Test". Adjust the capacity for the calculated range.
static const char PROFILE_P7PLUS[] PROGMEM = R"JSON({
  "name": "XPeng P7+ (experimental)",
  "model": "XPeng P7+",
  "battery_kwh": 74.9, "consumption": 16,
  "battery_options": [
    { "name": "61.7 kWh – Standard Range (usable 59.6 kWh)", "kwh": 59.6 },
    { "name": "74.9 kWh – Long Range / Performance", "kwh": 74.9 }
  ],
  "init": ["ATZ", "ATE0", "ATL0", "ATS0", "ATH0", "ATSP6", "ATAT1", "ATAL", "ATCRA784", "ATFCSH704", "ATFCSD300000", "ATFCSM1"],
  "pids": [
    { "id": "soc", "name": "SoC", "header": "704", "cmd": "221109",
      "formula": "u16(B3,B4)/10", "unit": "%", "device_class": "battery",
      "interval": 120, "min": 0, "max": 100, "precision": 1, "enabled": true },
    { "id": "bat_temp_max", "name": "Battery temp max", "header": "704", "cmd": "221107",
      "formula": "B3-40", "unit": "°C", "device_class": "temperature",
      "interval": 300, "min": -40, "max": 90, "precision": 0, "enabled": true },
    { "id": "bat_temp_min", "name": "Battery temp min", "header": "704", "cmd": "221108",
      "formula": "B3-40", "unit": "°C", "device_class": "temperature",
      "interval": 300, "min": -40, "max": 90, "precision": 0, "enabled": true },
    { "id": "odometer", "name": "Odometer", "header": "704", "cmd": "220101",
      "formula": "B3*65536+B4*256+B5", "unit": "km", "device_class": "distance",
      "state_class": "total_increasing", "icon": "mdi:counter",
      "interval": 1800, "min": 1, "max": 2000000, "precision": 0, "enabled": true },
    { "id": "range_cltc", "name": "Range (CLTC)", "header": "704", "cmd": "221118",
      "formula": "u16(B3,B4)", "unit": "km", "device_class": "distance",
      "icon": "mdi:map-marker-distance",
      "interval": 300, "min": 0, "max": 1000, "precision": 0, "enabled": true },
    { "id": "range_calc", "name": "Range (calculated)", "header": "704", "cmd": "221109",
      "formula": "u16(B3,B4)/10*CAP/CONS", "unit": "km", "device_class": "distance",
      "icon": "mdi:map-marker-distance",
      "interval": 120, "min": 0, "max": 800, "precision": 0, "enabled": true },
    { "id": "soh", "name": "SoH", "header": "704", "cmd": "22110A",
      "formula": "u16(B3,B4)/10", "unit": "%", "icon": "mdi:battery-heart-variant",
      "interval": 3600, "min": 0, "max": 100, "precision": 1, "enabled": true },
    { "id": "charge_limit", "name": "Charge limit", "header": "704", "cmd": "221130",
      "formula": "u16(B3,B4)-10", "unit": "%", "icon": "mdi:battery-charging-high",
      "interval": 900, "min": 0, "max": 100, "precision": 0, "enabled": true },
    { "id": "charge_status", "name": "Charging status (raw, 0 = not charging)", "header": "704", "cmd": "22112D",
      "formula": "B3", "state_class": "", "icon": "mdi:ev-station",
      "interval": 120, "min": 0, "max": 10, "precision": 0, "enabled": true, "sleep_zero": true },
    { "id": "hv_voltage", "name": "HV voltage", "header": "704", "cmd": "221101",
      "formula": "u16(B3,B4)/10", "unit": "V", "device_class": "voltage",
      "interval": 120, "min": 0, "max": 1000, "precision": 1, "enabled": true },
    { "id": "hv_current", "name": "HV current (negative = charging)", "header": "704", "cmd": "221103",
      "formula": "u16(B3,B4)*0.5-1600", "unit": "A", "device_class": "current",
      "interval": 120, "min": -1600, "max": 1600, "precision": 1, "enabled": true, "sleep_zero": true },
    { "id": "charge_power", "name": "Charging power", "header": "704", "cmd": "221103",
      "formula": "max(0,-(u16(B3,B4)*0.5-1600)*hv_voltage/1000)", "unit": "kW", "device_class": "power",
      "interval": 120, "min": 0, "max": 500, "precision": 1, "enabled": true, "sleep_zero": true },
    { "id": "charging", "name": "Charging", "header": "704", "cmd": "221103",
      "formula": "lt(u16(B3,B4)*0.5-1600,-2)", "icon": "mdi:ev-station", "state_class": "",
      "interval": 120, "min": 0, "max": 1, "precision": 0, "enabled": true, "sleep_zero": true },
    { "id": "charging_dc", "name": "DC charging", "header": "704", "cmd": "221103",
      "formula": "gt(-(u16(B3,B4)*0.5-1600)*hv_voltage/1000,11.5)", "icon": "mdi:ev-plug-ccs2", "state_class": "",
      "interval": 120, "min": 0, "max": 1, "precision": 0, "enabled": true, "sleep_zero": true }
  ]
})JSON";


struct DefaultProfile { const char* file; const char* json; };
static const DefaultProfile DEFAULT_PROFILES[] = {
  { "ioniq5", PROFILE_IONIQ5 },
  { "g9",     PROFILE_G9 },
  { "g6",     PROFILE_G6 },
  { "p7plus", PROFILE_P7PLUS },
};
