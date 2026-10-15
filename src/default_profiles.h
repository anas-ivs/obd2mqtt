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
// Tyres (TPMS, header 7A0, 22C00B): pressure raw×0.2 psi → bar, temperature raw−50 °C (evDash, matches WiCAN);
//   0xFF (sensor not seen) = 3.52 bar → rejected by max 3.5
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
      "interval": 120, "min": 0, "max": 1, "precision": 0, "enabled": true, "sleep_zero": true },
    { "id": "tyre_p_fl", "name": "Tyre pressure front left", "header": "7A0", "cmd": "22C00B",
      "formula": "B7*0.2/14.5038", "unit": "bar", "device_class": "pressure", "icon": "mdi:car-tire-alert",
      "interval": 300, "min": 0.5, "max": 3.5, "precision": 2, "enabled": true },
    { "id": "tyre_p_fr", "name": "Tyre pressure front right", "header": "7A0", "cmd": "22C00B",
      "formula": "B12*0.2/14.5038", "unit": "bar", "device_class": "pressure", "icon": "mdi:car-tire-alert",
      "interval": 300, "min": 0.5, "max": 3.5, "precision": 2, "enabled": true },
    { "id": "tyre_p_rl", "name": "Tyre pressure rear left", "header": "7A0", "cmd": "22C00B",
      "formula": "B17*0.2/14.5038", "unit": "bar", "device_class": "pressure", "icon": "mdi:car-tire-alert",
      "interval": 300, "min": 0.5, "max": 3.5, "precision": 2, "enabled": true },
    { "id": "tyre_p_rr", "name": "Tyre pressure rear right", "header": "7A0", "cmd": "22C00B",
      "formula": "B22*0.2/14.5038", "unit": "bar", "device_class": "pressure", "icon": "mdi:car-tire-alert",
      "interval": 300, "min": 0.5, "max": 3.5, "precision": 2, "enabled": true },
    { "id": "tyre_t_fl", "name": "Tyre temp front left", "header": "7A0", "cmd": "22C00B",
      "formula": "B8-50", "unit": "°C", "device_class": "temperature",
      "interval": 300, "min": -40, "max": 120, "precision": 0, "enabled": true },
    { "id": "tyre_t_fr", "name": "Tyre temp front right", "header": "7A0", "cmd": "22C00B",
      "formula": "B13-50", "unit": "°C", "device_class": "temperature",
      "interval": 300, "min": -40, "max": 120, "precision": 0, "enabled": true },
    { "id": "tyre_t_rl", "name": "Tyre temp rear left", "header": "7A0", "cmd": "22C00B",
      "formula": "B18-50", "unit": "°C", "device_class": "temperature",
      "interval": 300, "min": -40, "max": 120, "precision": 0, "enabled": true },
    { "id": "tyre_t_rr", "name": "Tyre temp rear right", "header": "7A0", "cmd": "22C00B",
      "formula": "B23-50", "unit": "°C", "device_class": "temperature",
      "interval": 300, "min": -40, "max": 120, "precision": 0, "enabled": true }
  ]
})JSON";

struct DefaultProfile { const char* file; const char* json; };
static const DefaultProfile DEFAULT_PROFILES[] = {
  { "ioniq5", PROFILE_IONIQ5 },
};
