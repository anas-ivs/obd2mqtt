#pragma once
#include <Arduino.h>

static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>OBD2MQTT</title>
<style>
:root{--bg:#f4f5f7;--card:#fff;--fg:#1d2330;--mut:#6b7385;--bd:#dfe3ea;--acc:#1f6feb;--ok:#1a7f37;--err:#cf222e;--warn:#9a6700;--in:#fff;--sleep:#5b6b8a;--term:#0f141b;--termfg:#cfe3d4;--termmut:#7f8b99}
@media(prefers-color-scheme:dark){:root{--bg:#0f1218;--card:#181c24;--fg:#e6e9ef;--mut:#8b93a5;--bd:#2a303c;--acc:#4c8dff;--ok:#3fb950;--err:#f85149;--warn:#d29922;--in:#11151c;--sleep:#8ea0c4;--term:#07090d;--termfg:#cfe3d4;--termmut:#6f7b89}}
*{box-sizing:border-box}body{margin:0;font:15px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--fg)}
header{display:flex;align-items:center;gap:12px;padding:12px 16px;background:var(--card);border-bottom:1px solid var(--bd);position:sticky;top:0;z-index:5;flex-wrap:wrap}
header h1{font-size:17px;margin:0;font-weight:650}header .sub{color:var(--mut);font-size:13px}
nav{display:flex;gap:4px;margin-left:auto;flex-wrap:wrap}
nav button{background:none;border:0;color:var(--mut);padding:7px 11px;border-radius:7px;cursor:pointer;font:inherit}
nav button.on{background:var(--bg);color:var(--fg);font-weight:600}
main{max-width:1100px;margin:0 auto;padding:16px}
.card{background:var(--card);border:1px solid var(--bd);border-radius:12px;padding:16px;margin-bottom:16px}
.card h2{font-size:15px;margin:0 0 12px}
.grid{display:grid;gap:12px;grid-template-columns:repeat(auto-fit,minmax(170px,1fr))}
.val{border:1px solid var(--bd);border-radius:10px;padding:12px}
.val .l{color:var(--mut);font-size:13px}.val .v{font-size:30px;font-weight:650;font-variant-numeric:tabular-nums}
.val .v small{font-size:15px;color:var(--mut);font-weight:500;margin-left:3px}.val .m{font-size:12px;color:var(--mut)}
.val .e{font-size:12px;color:var(--err);margin-top:4px;word-break:break-word}
.pill{display:inline-flex;align-items:center;gap:6px;font-size:13px;padding:3px 9px;border-radius:99px;border:1px solid var(--bd)}
.dot{width:8px;height:8px;border-radius:50%;background:var(--mut)}.dot.ok{background:var(--ok)}.dot.err{background:var(--err)}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
label{display:block;font-size:13px;color:var(--mut);margin:0 0 4px}
.f{display:grid;gap:12px;grid-template-columns:repeat(auto-fit,minmax(220px,1fr))}
input,select,textarea{width:100%;font:inherit;padding:7px 9px;border:1px solid var(--bd);border-radius:7px;background:var(--in);color:var(--fg)}
textarea{font-family:ui-monospace,Menlo,Consolas,monospace;font-size:13px}
input[type=checkbox]{width:auto}
button.b,a.b{display:inline-block;font:inherit;font-size:15px;line-height:1.45;padding:7px 13px;border-radius:7px;border:1px solid var(--bd);background:var(--card);color:var(--fg);cursor:pointer}
button.p,a.p{background:var(--acc);border-color:var(--acc);color:#fff}button.d,a.d{color:var(--err)}
button:disabled{opacity:.5;cursor:default}
.hint{color:var(--mut);font-size:13px;margin:6px 0 0}
pre,.mono{font-family:ui-monospace,Menlo,Consolas,monospace;font-size:12.5px}
pre{background:var(--in);border:1px solid var(--bd);border-radius:8px;padding:10px;overflow:auto;max-height:320px;margin:0;white-space:pre-wrap;word-break:break-all}
.tw{overflow-x:auto}table{border-collapse:collapse;width:100%;font-size:13px}
th,td{padding:5px 4px;border-bottom:1px solid var(--bd);text-align:left;vertical-align:middle}th{color:var(--mut);font-weight:500;white-space:nowrap}
td input,td select{padding:4px 6px;font-size:13px;min-width:44px}
td input[type=checkbox]{min-width:0;width:18px;height:18px}td select{min-width:100px}
.bytes{display:flex;flex-wrap:wrap;gap:3px}.bytes span{border:1px solid var(--bd);border-radius:5px;padding:2px 4px;text-align:center;min-width:44px}
.bytes b{display:block;font-size:10px;color:var(--mut);font-weight:500}
.toast{position:fixed;bottom:16px;left:50%;transform:translateX(-50%);background:var(--fg);color:var(--bg);padding:9px 16px;border-radius:9px;opacity:0;transition:.25s;pointer-events:none;z-index:9}
.toast.show{opacity:1}.hide{display:none!important}
.scanl div{display:flex;justify-content:space-between;gap:8px;padding:7px 4px;border-bottom:1px solid var(--bd);cursor:pointer}
.scanl div:hover{background:var(--bg)}
/* header: title + WiFi/MQTT + help; tabs at the top on wide screens, as a bottom bar on phones */
#conn{margin-left:auto;display:flex;gap:4px;flex-wrap:wrap;align-items:center}
#conn a{color:var(--acc);text-decoration:none;font-size:13px;padding:3px 6px}
nav{width:100%;margin:0}nav .ic{display:none}
@media(min-width:721px){nav{width:auto;order:2}#conn{order:3}}
@media(max-width:720px){
 body{padding-bottom:calc(62px + env(safe-area-inset-bottom,0px))}
 header{padding:10px 12px;gap:8px}
 nav{position:fixed;left:0;right:0;bottom:0;display:grid;grid-template-columns:repeat(5,1fr);gap:0;background:var(--card);border-top:1px solid var(--bd);padding-bottom:env(safe-area-inset-bottom,0px);z-index:6}
 nav button{display:flex;flex-direction:column;align-items:center;gap:2px;font-size:11px;padding:7px 2px 9px;border-radius:0}
 nav button.on{background:none;color:var(--acc)}
 nav .ic{display:block;width:20px;height:20px;stroke:currentColor;fill:none;stroke-width:1.8}
 main{padding:10px}.card{padding:12px;margin-bottom:10px}
 button.b,a.b{font-size:14px;padding:8px 12px}
 .toast{bottom:calc(74px + env(safe-area-inset-bottom,0px))}
}
/* live log */
.term{background:var(--term);color:var(--termfg);border-radius:12px;padding:10px 12px;margin-bottom:12px}
.term .bar{display:flex;justify-content:space-between;align-items:center;gap:6px;flex-wrap:wrap;margin-bottom:6px}
.term .bar h2{margin:0;font-size:14px;color:var(--termfg)}
.term .bar button,.term .bar a{font:inherit;font-size:12px;background:none;border:1px solid #2c3a49;color:var(--termfg);border-radius:6px;padding:3px 8px;cursor:pointer;text-decoration:none}
.term pre{background:none;border:0;padding:0;color:var(--termfg);max-height:150px;font-size:12px;line-height:1.5;word-break:break-word}
.term.big pre{max-height:60vh}
.term .ts{color:var(--termmut)}.term .k0{color:#8fd3ff}.term .k1{color:#ffcf7a}.term .er{color:#ff9c9c}
/* car cards */
.car{background:var(--card);border:1px solid var(--bd);border-radius:12px;margin-bottom:12px;overflow:hidden}
.car-h{padding:12px 14px 6px;display:flex;justify-content:space-between;gap:8px;align-items:flex-start}
.car-h .n{font-weight:700;font-size:16px}.car-h .sub{font-size:12px;color:var(--mut)}
.badge{font-size:12px;font-weight:600;padding:3px 10px;border-radius:99px;white-space:nowrap;background:var(--bg);color:var(--mut)}
.badge.sl{color:var(--sleep)}.badge.ch{color:var(--ok)}.badge.aw{color:var(--acc)}
.tiles{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:8px;padding:4px 14px 8px}
.tile{border:1px solid var(--bd);border-radius:10px;padding:8px 10px;min-width:0}
.tile .l{font-size:12px;color:var(--mut)}
.tile .v{font-size:22px;font-weight:650;font-variant-numeric:tabular-nums;line-height:1.25;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.tile .v small{font-size:12px;color:var(--mut);font-weight:500;margin-left:2px}.tile .v.sm{font-size:16px}
.more{display:block;margin:0 14px 8px;font:inherit;font-size:13px;color:var(--acc);background:none;border:0;padding:4px 0;cursor:pointer}
.allv{display:grid;gap:8px;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));padding:0 14px 12px}
.allv .val{padding:8px 10px}.allv .val .v{font-size:18px}.allv .val .v small{font-size:12px}
.obd{border-top:1px solid var(--bd);padding:8px 14px 12px;background:color-mix(in srgb,var(--bg) 55%,var(--card))}
.obd .r1{display:flex;align-items:center;gap:6px;font-size:13px;min-width:0}
.obd .r1 span:last-child{min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.obd .nx{font-size:12px;color:var(--mut);margin:3px 0 0 14px;word-break:break-word}
.leg{display:grid;grid-template-columns:repeat(auto-fill,minmax(230px,1fr));gap:4px 12px;font-size:12.5px;color:var(--mut);margin-top:6px}.leg i{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:6px;border:1px solid #8888;vertical-align:-1px}
.obd .r2{display:flex;gap:6px;margin-top:8px;flex-wrap:wrap;align-items:center}
.obd .err{font-size:12px;color:var(--err);margin-top:4px;word-break:break-word}
.dot.sl{background:var(--sleep)}
.sw{display:inline-flex;align-items:center;gap:6px;font-size:13px;margin-left:auto;cursor:pointer;user-select:none;background:none;border:0;color:var(--fg);font-family:inherit;padding:4px 0}
.sw i{width:36px;height:22px;border-radius:99px;background:var(--bd);position:relative;display:inline-block;transition:background .15s}
.sw i::after{content:"";position:absolute;top:3px;left:3px;width:16px;height:16px;border-radius:50%;background:#fff;transition:left .15s}
.sw[aria-checked="true"] i{background:var(--ok)}.sw[aria-checked="true"] i::after{left:17px}
.carpick{display:flex;gap:8px;align-items:center;margin-bottom:10px}.carpick label{margin:0}.carpick select{width:auto}
@media(prefers-reduced-motion:reduce){.sw i,.sw i::after,.toast{transition:none}}
</style></head><body>
<header><div><h1>OBD2MQTT</h1><div class="sub" id="hsub">BLE-OBD2 → MQTT bridge</div></div>
<div id="conn"></div>
<nav id="nav">
<button data-t="status" class="on"><svg class="ic" viewBox="0 0 24 24"><path d="M3 12h4l3-8 4 16 3-8h4"/></svg>Status</button>
<button data-t="profile"><svg class="ic" viewBox="0 0 24 24"><path d="M4 7h16M4 12h16M4 17h10"/></svg>Profile</button>
<button data-t="term"><svg class="ic" viewBox="0 0 24 24"><path d="M5 7l5 5-5 5M12 17h7"/></svg>Terminal</button>
<button data-t="cfg"><svg class="ic" viewBox="0 0 24 24"><circle cx="12" cy="12" r="3"/><path d="M12 2v3M12 19v3M2 12h3M19 12h3M5 5l2 2M17 17l2 2M5 19l2-2M17 7l2-2"/></svg>Settings</button>
<button data-t="sys"><svg class="ic" viewBox="0 0 24 24"><rect x="4" y="4" width="16" height="16" rx="2"/><path d="M9 9h6v6H9z"/></svg>System</button>
</nav></header>
<main>

<section id="t-status">
 <div class="card hide" id="safebox" style="border-color:var(--err)"><h2 style="color:var(--err)">Safe mode</h2>
  <p style="margin:0">The firmware crashed several times in a row. Bluetooth and polling are therefore disabled, the web interface keeps running. Check profile and settings, then restart normally.</p>
  <div class="row" style="margin-top:10px"><button class="b p" onclick="api('/api/reboot',{method:'POST'}).then(r=>toast(r.msg))">Restart normally</button></div></div>
 <div class="term" id="term">
  <div class="bar"><h2>Live log</h2><div class="row" style="gap:4px"><button onclick="logBig()" id="logbigbtn">Full log</button><button onclick="copyLog()">Copy</button><a href="/api/log?dl=1">Download</a><a href="/api/log?old=1&amp;dl=1">Older</a><button onclick="if(confirm('Delete log?'))api('/api/log-clear',{method:'POST'}).then(r=>{toast(r.msg);status()})">Delete</button></div></div>
  <pre id="log"></pre></div>
 <div id="carcards"></div>
</section>

<section id="t-profile" class="hide">
 <div class="card"><div class="row">
  <div style="flex:1;min-width:160px"><label>Profile</label><select id="psel" onchange="loadProfile(this.value)"></select></div>
  <div style="align-self:end" class="row"><span id="usebtns" class="row"></span><span class="carpick" style="margin:0"><label for="pcar">Test with</label><select id="pcar" class="carsel"></select></span><button class="b" onclick="saveAs()">Save copy…</button><button class="b d" onclick="delProfile()">Delete</button></div></div>
  <p class="hint" id="pact"></p></div>
 <div class="card"><h2>General</h2><div class="f">
  <div><label>Name</label><input id="p_name"></div>
  <div><label>Model (for Home Assistant)</label><input id="p_model"></div>
  <div><label>Init commands (one per line)</label><textarea id="p_init" rows="4"></textarea></div></div></div>
 <div class="card"><h2>Battery &amp; range</h2><div class="f">
  <div><label>Battery variant</label><select id="p_batsel" onchange="batSel()"></select></div>
  <div><label>Usable capacity (kWh)</label><input id="p_bat" type="number" step="0.1" min="0" oninput="batInput()"></div>
  <div><label>Consumption (kWh/100 km)</label><input id="p_cons" type="number" step="0.1" min="0" oninput="batCalc()"></div></div>
  <p class="hint" id="p_batinfo"></p>
  <p class="hint">Formulas can use <span class="mono">CAP</span> (usable kWh) and <span class="mono">CONS</span> (kWh/100 km), e.g. calculated range <span class="mono">B34/2*CAP/CONS</span> (SoC × kWh ÷ kWh/100 km). Pick your battery variant – the value can be corrected by hand. Changes take effect after <b>Save profile</b>.</p></div>
 <div class="card"><div class="row" style="justify-content:space-between"><h2 style="margin:0">PIDs</h2><button class="b" onclick="addPid()">+ PID</button></div>
  <p class="hint">Formula: <span class="mono">B0..Bn</span> are the response bytes <b>including</b> service bytes (for <span class="mono">220105</span>: B0=62, B1=01, B2=05). Functions: <span class="mono">u16(hi,lo) s16(hi,lo) s8(x) bit(x,n) lt(a,b) gt(a,b) min(a,b) max(a,b)</span>, plus <span class="mono">CAP CONS</span> and the last value of any other value by its ID (e.g. <span class="mono">hv_voltage</span>). Use <b>Test</b> to see all bytes with their index.</p>
  <div class="tw"><table><thead><tr><th><label style="display:flex;gap:4px;align-items:center;margin:0;color:inherit"><input type="checkbox" id="allpids" onchange="toggleAll(this.checked)" title="All on/off"> <span>Active</span></label></th><th>ID</th><th>Name</th><th>Header</th><th>Command</th><th>Formula</th><th>Unit</th><th>Class</th><th>Interval s</th><th>Min</th><th>Max</th><th>Dec</th><th></th></tr></thead><tbody id="pids"></tbody></table></div>
  <div class="row" style="margin-top:12px"><button class="b p" onclick="saveProfile()">Save profile</button><button class="b" onclick="toggleRaw()">Edit JSON</button></div>
  <div id="rawbox" class="hide" style="margin-top:12px"><textarea id="raw" rows="16"></textarea><div class="row" style="margin-top:8px"><button class="b" onclick="rawApply()">Apply JSON</button></div></div>
 </div>
 <div class="card hide" id="testcard"><h2>Test result <span id="testid" class="mono"></span></h2><div id="testout"></div></div>
</section>

<section id="t-term" class="hide">
 <div class="card"><h2>ELM327 terminal</h2>
  <div class="carpick"><label for="tcar">Car</label><select id="tcar" class="carsel"></select></div>
  <div class="row"><input id="tcmd" class="mono" style="flex:1;min-width:180px" placeholder="e.g. ATRV, ATSH7E4, 220105" onkeydown="if(event.key==='Enter')termSend()"><button class="b p" onclick="termSend()">Send</button></div>
  <div class="row" style="margin-top:8px"><span class="hint" style="margin:0">Quick:</span>
   <button class="b" onclick="q('ATI')">ATI</button><button class="b" onclick="q('ATRV')">ATRV</button><button class="b" onclick="q('ATDP')">ATDP</button><button class="b" onclick="q('0100')">0100</button><button class="b" onclick="q('ATSH7E4')">ATSH7E4</button><button class="b" onclick="q('220105')">220105</button></div>
  <pre id="tout" style="margin-top:12px;min-height:200px"></pre>
  <p class="hint">Commands go to the dongle of the chosen car and run between the automatic polls. After ATSH commands the poller sets the header again by itself.</p></div>
</section>

<section id="t-cfg" class="hide">
 <div class="card"><h2>WiFi &amp; access</h2><div class="f">
  <div><label>SSID</label><div class="row" style="flex-wrap:nowrap"><input id="wifi_ssid"><button class="b" id="wscanbtn" onclick="wscan()" style="white-space:nowrap">Scan</button></div></div>
  <div><label>Password</label><input id="wifi_pass" type="password" autocomplete="new-password"></div>
  <div><label>Hostname</label><input id="hostname"></div>
  <div><label>Web password (user admin, empty = none)</label><input id="web_pass" type="password" autocomplete="new-password"></div>
  <div><label>IP address</label><select id="ip_mode" onchange="ipModeChange()"><option value="dhcp">automatic (DHCP)</option><option value="static">static IP address</option></select></div></div>
  <div class="scanl" id="wscanl" style="margin-top:8px"></div>
  <div id="ipbox" class="hide" style="margin-top:10px"><div class="f">
   <div><label>Static IP address</label><input id="ip_addr" class="mono" placeholder="192.168.2.50"></div>
   <div><label>Subnet mask</label><input id="ip_mask" class="mono" placeholder="255.255.255.0"></div>
   <div><label>Gateway (router)</label><input id="ip_gw" class="mono" placeholder="192.168.2.1"></div>
   <div><label>DNS server (empty = gateway)</label><input id="ip_dns1" class="mono"></div>
   <div><label>2nd DNS server (optional)</label><input id="ip_dns2" class="mono"></div></div>
   <div class="row" style="margin-top:8px"><button class="b" onclick="ipTakeCurrent()">Use current values</button></div>
   <p class="hint">The address must be outside your router's DHCP range. If the static IP does not work after the restart (no WiFi connection or MQTT broker not reachable), the bridge automatically gets an address via DHCP and reports this in the log and under System.</p>
   <p class="hint" id="ipfb" style="color:var(--err)"></p></div></div>
 <div class="card"><h2>Cars</h2>
  <p class="hint" style="margin:0 0 10px">One dongle per car. The ESP32 connects to one car at a time and serves whichever car is due. Each car appears as its own device in Home Assistant under its base topic.</p>
  <div id="carcfg"></div>
  <div class="row" style="margin-top:12px"><button class="b" id="scanbtn" onclick="scan()">Scan for dongles</button></div>
  <div class="scanl" id="scanl" style="margin-top:8px"></div></div>
 <div class="card"><h2>Polling</h2><div class="f">
  <div><label>12V minimum voltage (0 = off)</label><input id="min_voltage" type="number" step="0.1"></div>
  <div><label>Car asleep below (V, 0 = off)</label><input id="sleep_voltage" type="number" step="0.1"></div>
  <div><label>Poll while car is asleep (min, 0 = never)</label><input id="sleep_poll_min" type="number"></div>
  <div><label>Check 12V while car is asleep every (min)</label><input id="sleep_check_min" type="number" min="1" max="240"></div>
  <div><label>12V warning below (V, 0 = off)</label><input id="low_batt_voltage" type="number" step="0.1"></div>
  <div><label>Poll interval while charging, AC or DC (s, 0 = profile)</label><input id="charge_interval_sec" type="number" min="0"></div>
  <div><label>Poll interval when awake, not charging (s, 0 = profile)</label><input id="normal_interval_sec" type="number" min="0"></div>
  <div><label>Awake, not charging: polls at that interval, then slower</label><input id="awake_fast_polls" type="number" min="0" max="100"></div>
  <div><label>… then poll every (s, 0 = never slower)</label><input id="awake_slow_sec" type="number" min="0"></div>
  <div><label>Dongle not found – next attempt after (min)</label><input id="dongle_retry_min" type="number" min="1" max="240"></div>
  <div><label>Pause when car does not respond (s, 0 = off)</label><input id="backoff_sec" type="number"></div>
  <div><label>NTP server (time)</label><input id="ntp_server" placeholder="pool.ntp.org or fritz.box"></div>
  <div><label>Time zone</label><select id="tzsel" onchange="tzChange()"><option value="MYT-8">Malaysia, Singapore</option><option value="CET-1CEST,M3.5.0,M10.5.0/3">Central Europe – Germany, Austria, Switzerland, Italy, France …</option><option value="GMT0BST,M3.5.0/1,M10.5.0">United Kingdom, Ireland</option><option value="WET0WEST,M3.5.0/1,M10.5.0">Portugal, Canary Islands</option><option value="EET-2EEST,M3.5.0/3,M10.5.0/4">Eastern Europe – Finland, Greece, Romania, Baltics …</option><option value="&lt;+03&gt;-3">Turkey</option><option value="MSK-3">Moscow</option><option value="UTC0">UTC (no daylight saving)</option><option value="EST5EDT,M3.2.0,M11.1.0">USA/Canada – Eastern</option><option value="CST6CDT,M3.2.0,M11.1.0">USA/Canada – Central</option><option value="MST7MDT,M3.2.0,M11.1.0">USA/Canada – Mountain</option><option value="PST8PDT,M3.2.0,M11.1.0">USA/Canada – Pacific</option><option value="CST-8">China</option><option value="JST-9">Japan</option><option value="AEST-10AEDT,M10.1.0,M4.1.0/3">Australia – Sydney, Melbourne</option><option value="custom">Other (POSIX string) …</option></select><input id="tz" class="mono hide" style="margin-top:6px" placeholder="e.g. CET-1CEST,M3.5.0,M10.5.0/3"><p class="hint" id="devtime"></p></div>
  <div><label>Store log permanently (flash)</label><select id="log_persist"><option value="true">yes – survives restarts</option><option value="false">no – RAM only</option></select></div>
  <div><label>Check daily for new firmware (GitHub)</label><select id="update_check"><option value="true">yes</option><option value="false">no</option></select></div>
  <div><label>Command timeout (ms)</label><input id="cmd_timeout_ms" type="number"></div>
  <div><label>Bluetooth transmit power</label><select id="ble_tx_power"><option value="3">+3 dBm</option><option value="6">+6 dBm</option><option value="9">+9 dBm (ESP32 maximum)</option><option value="12">+12 dBm</option><option value="15">+15 dBm</option><option value="18">+18 dBm</option><option value="20">+20 dBm (ESP32-S3)</option></select></div>
  <div><label>Keep BLE connected permanently</label><select id="keep_connected"><option value="false">no – dongle may sleep</option><option value="true">yes</option></select></div></div></div>
 <div class="card"><h2>Status LED</h2><div class="f">
  <div><label>LED GPIO (-1 = none)</label><div class="row" style="flex-wrap:nowrap"><input id="led_pin" type="number" min="-1" max="48"><button class="b" type="button" onclick="ledTest()">Test</button></div></div>
  <div><label>Brightness (%, 0 = off)</label><input id="led_brightness" type="number" min="0" max="100"></div></div>
  <p class="hint">RGB LED on ESP32-S3/C3 boards: usually GPIO48 on the S3 (some boards 38), GPIO8 on the C3. <b>Test</b> lights the entered GPIO red, green, blue – no saving needed.</p><div class="leg"><span><i style="background:#3b82f6"></i>Blue, slow blink – Connecting to the dongle</span><span><i style="background:#22c55e"></i>Green, flash per reply – Polling</span><span><i style="background:#15803d"></i>Dim green – Idle between polls</span><span><i style="background:#1e40af"></i>Short blue pulse every 5 s – Car asleep</span><span><i style="background:#eab308"></i>Yellow – Dongle not found / no data from an awake car</span><span><i style="background:#f97316"></i>Orange – WiFi OK, MQTT broker not connected</span><span><i style="background:#e5e7eb"></i>White, slow blink – No WiFi connection</span><span><i style="background:#a855f7"></i>Purple, slow blink – Setup hotspot active</span><span><i style="background:#06b6d4"></i>Cyan, fast blink – Firmware update running</span><span><i style="background:#ef4444"></i>Red, fast blink – Safe mode after repeated crashes</span></div><p class="hint"><a href="/help#led" target="_blank" style="color:var(--acc);text-decoration:none">LED colours in the manual ↗</a></p></div>
 <div class="card"><h2>MQTT / Home Assistant</h2><div class="f">
  <div><label>Broker</label><input id="mqtt_host" placeholder="192.168.1.10"></div>
  <div><label>Port</label><input id="mqtt_port" type="number"></div>
  <div><label>User</label><input id="mqtt_user" autocomplete="off"></div>
  <div><label>Password</label><input id="mqtt_pass" type="password" autocomplete="new-password"></div>
  <div><label>Bridge base topic (status, firmware update)</label><input id="mqtt_base"></div>
  <div><label>Discovery prefix</label><input id="ha_prefix"></div></div>
  <p class="hint">The base topic and Home Assistant device name of each car are set under <b>Cars</b>.</p></div>
 <div class="row"><button class="b p" onclick="saveCfg(false)">Save</button><button class="b" onclick="saveCfg(true)">Save &amp; restart</button>
  <span class="hint" style="margin:0">WiFi, MQTT and password changes take effect after the restart.</span></div>
</section>

<section id="t-sys" class="hide">
 <div class="card"><div class="row" style="justify-content:space-between"><h2 style="margin:0">Diagnostics</h2>
  <div class="row"><span class="carpick"><label for="dcar">Car</label><select id="dcar" class="carsel"></select></span><button class="b" id="dbgbtn" onclick="toggleDebug()">Diagnostic log: OFF</button><button class="b p" id="diagbtn" onclick="runDiag()">Create diagnostic report</button></div></div>
  <p class="hint">The <b>diagnostic report</b> reconnects to the dongle, records every init response and queries every value of the profile once with raw data (takes up to approx. 1 min, wakes the car). It contains no WiFi names, IP addresses or passwords and can therefore be posted e.g. in the forum. The <b>diagnostic log</b> additionally writes every request with its raw response to the log for 2 hours.</p>
  <div id="diagbox" class="hide" style="margin-top:10px"><div class="row"><button class="b" onclick="copyDiag()">Copy</button><button class="b" onclick="dlDiag()">Download</button></div>
  <pre id="diagout" style="max-height:400px;margin-top:10px"></pre></div></div>
 <div class="card"><h2>System</h2><div id="sysinfo" class="mono"></div>
  <div class="row" style="margin-top:12px"><button class="b" onclick="api('/api/reboot',{method:'POST'}).then(r=>toast(r.msg))">Restart</button>
  <button class="b d" onclick="if(confirm('Overwrite factory profiles (ioniq5, ev6)?'))api('/api/profile-defaults',{method:'POST'}).then(r=>{toast(r.msg);loadProfiles()})">Restore factory profiles</button></div></div>
 <div class="card"><h2>Backup &amp; restore</h2>
  <p class="hint" style="margin:0 0 10px">Saves all settings and vehicle profiles to a file – e.g. before re-flashing or to set up a second device.</p>
  <div class="row"><a class="b p" id="bkbtn" style="text-decoration:none" href="/api/backup?secrets=0">Download backup</a>
   <label style="margin:0;display:flex;gap:6px;align-items:center;color:var(--fg)"><input type="checkbox" id="bksec" onchange="$('bkbtn').href='/api/backup?secrets='+(this.checked?1:0)"> incl. passwords (WiFi, MQTT, web)</label></div>
  <p class="hint">Without passwords the file is safe to share; when restoring, the currently stored passwords are kept.</p>
  <div style="border-top:1px solid var(--bd);margin-top:12px;padding-top:12px">
   <input type="file" id="rsfile" accept=".json,application/json">
   <div class="row" style="margin-top:8px"><label style="margin:0;display:flex;gap:6px;align-items:center;color:var(--fg)"><input type="checkbox" id="rscfg" checked> Settings</label>
    <label style="margin:0;display:flex;gap:6px;align-items:center;color:var(--fg)"><input type="checkbox" id="rsprof" checked> Profiles</label>
    <button class="b" id="rsbtn" onclick="restore()">Restore</button><span id="rsst" class="hint" style="margin:0"></span></div></div></div>
 <div class="card"><h2>Changelog</h2><div id="changelog" style="max-height:360px;overflow:auto"></div></div>
 <div class="card"><h2>Online update</h2><p class="hint" id="oust" style="margin:0 0 10px">–</p>
  <div class="row"><button class="b" id="oucheck" onclick="ouCheck()">Check for updates</button><button class="b p hide" id="ouinst" onclick="ouInstall()">Install now</button><a class="hint hide" id="oulink" target="_blank" rel="noopener" style="margin:0">Release on GitHub ↗</a></div>
  <progress id="oubar" max="100" value="0" class="hide" style="width:100%;height:14px;margin-top:10px"></progress>
  <pre id="ounotes" class="hide" style="white-space:pre-wrap;max-height:300px;overflow:auto;margin-top:10px;font-size:13px"></pre>
  <p class="hint">Checks the latest release on GitHub (anas-ivs/obd2mqtt) and installs the matching *-ota.bin directly from the internet. Bluetooth pauses during the download, then the bridge restarts. In Home Assistant the same appears as update entity “Firmware”.</p></div>
 <div class="card"><h2>Firmware-Update</h2><p class="hint" id="fwcur" style="margin:0 0 10px"></p><input type="file" id="fw" accept=".bin"><div class="row" style="margin-top:8px"><button class="b p" id="otabtn" onclick="ota()">Upload</button><span id="otast" class="hint" style="margin:0"></span></div>
  <progress id="otabar" max="100" value="0" class="hide" style="width:100%;height:14px;margin-top:10px"></progress>
  <p class="hint">BLE polling pauses during the update. Afterwards the ESP32 restarts – the page waits and shows the new version.</p></div>
</section>
</main><div class="toast" id="toast"></div>
<script>
const $=id=>document.getElementById(id);
const esc=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const nl=s=>String(s).replace(/\r\n?/g,'\n');
let cur='status',prof=null,profName='',active=[];
let CARS=[];   // cars from /api/status
function toast(m){const t=$('toast');t.textContent=m;t.classList.add('show');clearTimeout(t._h);t._h=setTimeout(()=>t.classList.remove('show'),2600)}
async function api(u,o={}){const r=await fetch(u,o);let j;try{j=await r.json()}catch(e){j={ok:r.ok,msg:r.statusText}}if(!r.ok&&j.ok===undefined)j.ok=false;return j}
const post=(u,b)=>api(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)});
document.querySelectorAll('#nav button').forEach(b=>b.onclick=()=>show(b.dataset.t));
function show(t){cur=t;document.querySelectorAll('#nav button').forEach(b=>b.classList.toggle('on',b.dataset.t===t));
 document.querySelectorAll('main>section').forEach(s=>s.classList.toggle('hide',s.id!=='t-'+t));
 if(t==='profile'&&!prof)loadProfiles();if(t==='cfg')loadCfg();if(t==='sys'){status();ouShow().catch(()=>{})}}
// Next poll: clock time + countdown, refreshed every second by the ticker below
const left=s=>s<=0?'now':s<60?s+' s':s<600?Math.floor(s/60)+' min '+(s%60)+' s':s<3600?Math.round(s/60)+' min':Math.floor(s/3600)+' h '+Math.round(s%3600/60)+' min';
const nxtTxt=at=>{const s=Math.round((at-Date.now())/1000);return s<=0?'now':new Date(at).toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'})+' (in '+left(s)+')'};
setInterval(()=>document.querySelectorAll('.nxt').forEach(e=>e.textContent=nxtTxt(Number(e.dataset.at))),1000);
const age=s=>s<0?'never':s<60?s+' s':s<3600?Math.round(s/60)+' min':Math.round(s/3600)+' h';
function pill(ok,txt){return `<span class="pill"><span class="dot ${ok?'ok':'err'}"></span>${esc(txt)}</span>`}
let logSeq=-1;
// The log is fetched separately and only when it changed (log_seq) – keeps /api/status small
// Live log: car names in their colour, errors in red, newest line at the bottom
async function loadLog(){try{const j=await api('/api/logtail');logSeq=j.seq;const lg=$('log'),atB=lg.scrollTop+lg.clientHeight>=lg.scrollHeight-5;
 const names=CARS.map(c=>c.name);
 lg.innerHTML=j.lines.map(l=>{let h=esc(l);const m=l.match(/^(\S+ \S+) (.*)$/);if(m){const rest=m[2],k=rest.match(/^\[([^\]]+)\] (.*)$/),err=/error|fail|not reachable|not found|brownout|dropped/i.test(rest);
  h=`<span class="ts">${esc(m[1])}</span> `+(k?`<span class="k${Math.max(0,names.indexOf(k[1]))%2}">[${esc(k[1])}]</span> <span class="${err?'er':''}">${esc(k[2])}</span>`:`<span class="${err?'er':''}">${esc(rest)}</span>`)}return h}).join('\n');
 if(atB)lg.scrollTop=lg.scrollHeight}catch(e){}}
function logBig(){const t=$('term');t.classList.toggle('big');$('logbigbtn').textContent=t.classList.contains('big')?'Smaller':'Full log';const lg=$('log');lg.scrollTop=lg.scrollHeight}
const openCars=new Set();   // cars whose "all values" list is expanded
function toggleVals(i){openCars.has(i)?openCars.delete(i):openCars.add(i);status()}
const pv=(c,id)=>(c.pids||[]).find(p=>p.id===id&&p.enabled);
const num=p=>p&&p.value!==undefined?Number(p.value):NaN;
function carCard(c,i){
 const soc=pv(c,'soc'),chg=pv(c,'charging'),dc=pv(c,'charging_dc'),kw=num(pv(c,'charge_power'));
 const isDc=num(dc)===1,isAc=!isDc&&num(chg)===1,kwTxt=isNaN(kw)?'':` ${kw.toFixed(1)}<small>kW</small>`;
 const charging=isDc?'DC'+kwTxt:isAc?'AC'+kwTxt:(chg||dc)?'Off':'–';
 const badge=isDc?['ch','DC charging']:isAc?['ch','AC charging']:c.car_asleep?['sl','Asleep']:(c.last_data_s>=0&&c.last_data_s<900)?['aw','Awake']:['','No data'];
 const tiles=`<div class="tile"><div class="l">12V battery</div><div class="v">${c.voltage!==undefined?c.voltage.toFixed(1)+'<small>V</small>':'–'}</div></div>
  <div class="tile"><div class="l">SoC</div><div class="v">${soc&&soc.value!==undefined?soc.value+'<small>%</small>':'–'}</div></div>
  <div class="tile"><div class="l">Charging</div><div class="v sm">${charging}</div></div>`;
 const rest=(c.pids||[]).filter(p=>p.enabled&&!['soc','charging','charging_dc'].includes(p.id));
 const open=openCars.has(i);
 const all=open?`<div class="allv">${rest.map(p=>`<div class="val"><div class="l">${esc(p.name)}</div><div class="v">${p.value??'–'}<small>${esc(p.unit)}</small></div><div class="m">${age(p.age_s)} ago</div>${p.error?`<div class="e">${esc(p.error)}</div>`:''}</div>`).join('')}</div>`:'';
 const b=c.ble,dongle=esc(b.name||b.mac||'no dongle set up');
 const at=Date.now()+Math.max(0,c.next_poll_s||0)*1000,nx=`<span class="nxt" data-at="${at}">${nxtTxt(at)}</span>`;
 const iv=c.interval_s?` · every ${c.interval_s%60?c.interval_s+' s':c.interval_s/60+' min'}${c.charging_mode?' (charging)':''}`:'';
 const nxt=b.connected?'':{off:'Polling off',check:'Next 12V check '+nx,quick:`Retry ${c.quick_retry}/2 `+nx,retry:'Next attempt '+nx,poll:'Next poll '+nx+iv}[c.next_why]||'';
 const st=b.connected?`connected${b.rssi?' · '+b.rssi+' dBm':''}`:c.car_asleep?'asleep':'disconnected';
 return `<div class="car"><div class="car-h"><div><div class="n">${esc(c.name)}</div><div class="sub">${esc(c.profile_name||c.profile)} · last data ${c.last_data_s>=0?age(c.last_data_s)+' ago':'none since restart'}</div></div><span class="badge ${badge[0]}">${badge[1]}</span></div>
  <div class="tiles">${tiles}</div>
  <button class="more" onclick="toggleVals(${i})" aria-expanded="${open}">${open?'Hide values ▴':'Show all '+rest.length+' values ▾'}</button>${all}
  <div class="obd"><div class="r1"><span class="dot ${b.connected?'ok':c.car_asleep?'sl':'err'}"></span><span><b>OBD</b> ${dongle} · ${st}</span></div>${nxt?`<div class="nx">${nxt}</div>`:''}
   ${c.last_error?`<div class="err">Last error: ${esc(c.last_error)}</div>`:''}
   <div class="r2"><button class="b" onclick="pollNow(${i})" title="Polls all values now if the car is awake – a sleeping car is not woken (12 V check only)">Poll now</button>
    <button class="sw" role="switch" aria-checked="${!!c.poll_enabled}" onclick="togglePoll(${i},${!c.poll_enabled})"><i></i>Polling</button></div></div></div>`}
const carLabel=i=>(CARS[i]&&CARS[i].name)||('Car '+(i+1));
// Car pickers next to Terminal, Test and Diagnostics (enabled cars only)
function fillCarSelects(){const en=CARS.map((c,i)=>i).filter(i=>CARS[i].enabled);
 const h=en.map(i=>`<option value="${i}">${esc(carLabel(i))}</option>`).join('');
 document.querySelectorAll('.carsel').forEach(sel=>{if(sel.dataset.h!==h){const v=sel.value;sel.innerHTML=h;sel.dataset.h=h;if(en.includes(Number(v)))sel.value=v}})}
const selOf=id=>Number(($(id)&&$(id).value)||0);
async function status(){try{const s=await api('/api/status');CARS=s.cars||[];fillCarSelects();
 if(s.ble_tx_max)document.querySelectorAll('#ble_tx_power option').forEach(o=>o.hidden=Number(o.value)>s.ble_tx_max);
 const en=CARS.map((c,i)=>i).filter(i=>CARS[i].enabled);
 $('hsub').textContent='v'+s.fw+' · '+(s.board||'')+' · '+(s.wifi.ip||'');if($('fwcur'))$('fwcur').textContent='Installed: v'+s.fw;
 $('carcards').innerHTML=en.map(i=>carCard(CARS[i],i)).join('')||'<div class="card"><p class="hint" style="margin:0">No car enabled – set one up under Settings → Cars.</p></div>';
 $('conn').innerHTML=pill(s.wifi.connected,'WiFi'+(s.wifi.connected?' '+s.wifi.rssi+' dBm':''))+pill(s.mqtt,'MQTT')+'<a href="/help" target="_blank">Help ↗</a>';
 if(s.debug_min!==undefined){dbgMin=s.debug_min;dbgBtn()}
 $('safebox').classList.toggle('hide',!s.safe_mode);
 if(s.log_seq!==logSeq&&cur==='status')loadLog();
 if($('devtime'))$('devtime').textContent='Time on the device'+': '+(s.time||'not synchronized yet');
 $('sysinfo').innerHTML=`Firmware ${esc(s.fw)}<br>${'Last restart'}: ${esc(s.reset_reason||'')}<br>${'Uptime'} ${age(s.uptime)}<br>${'Free heap'} ${Math.round(s.heap/1024)} KB (${'minimum'} ${Math.round((s.heap_min||0)/1024)} KB, ${'largest block'} ${Math.round((s.heap_block||0)/1024)} KB)<br>${'File system'} ${Math.round((s.fs_used||0)/1024)} / ${Math.round((s.fs_total||0)/1024)} KB<br>${'Poll cycles'} ${CARS.filter(c=>c.enabled).map(c=>esc(c.name)+' '+c.cycles).join(', ')}<br>IP ${esc(s.wifi.ip)} (${s.wifi.ip_mode==='static'?'static':s.wifi.ip_mode==='fallback'?'DHCP fallback – static IP did not work':'DHCP'})`;
 if($('ipfb'))$('ipfb').textContent=s.wifi.ip_mode==='fallback'?'The static IP did not work at the last start – currently via DHCP: '+s.wifi.ip:'';
}catch(e){}}
async function copyLog(){let txt='';try{txt=await (await fetch('/api/log')).text()}catch(e){txt=$('log').textContent}
 let ok=false;try{if(navigator.clipboard&&window.isSecureContext){await navigator.clipboard.writeText(txt);ok=true}}catch(e){}
 if(!ok){const ta=document.createElement('textarea');ta.value=txt;ta.style.position='fixed';ta.style.opacity='0';document.body.appendChild(ta);ta.select();try{ok=document.execCommand('copy')}catch(e){}ta.remove()}
 toast(ok?'Log copied to clipboard ('+txt.split('\n').length+' lines)':'Copy not possible – please use “Download”')}
async function copyText(txt){let ok=false;try{if(navigator.clipboard&&window.isSecureContext){await navigator.clipboard.writeText(txt);ok=true}}catch(e){}
 if(!ok){const ta=document.createElement('textarea');ta.value=txt;ta.style.position='fixed';ta.style.opacity='0';document.body.appendChild(ta);ta.select();try{ok=document.execCommand('copy')}catch(e){}ta.remove()}return ok}
async function runDiag(){const dc=selOf('dcar');if(!confirm('Create diagnostic report for '+carLabel(dc)+'? The car will be woken up and every value queried once (approx. 1 min).'))return;
 const btn=$('diagbtn');btn.disabled=true;btn.textContent='Running…';
 const r=await runJob({type:'diag',car:dc},450,p=>{btn.textContent='Running…'+' '+p});btn.disabled=false;btn.textContent='Create diagnostic report';if(!r)return;
 let txt='';if(r.ok){try{txt=await (await fetch('/api/diag')).text()}catch(e){txt='Error: '+e}}else txt='Error: '+(r.error||'');
 $('diagout').textContent=txt;$('diagbox').classList.remove('hide')}
async function copyDiag(){toast(await copyText($('diagout').textContent)?'Report copied to clipboard':'Copy not possible – please use “Download”')}
function dlDiag(){location.href='/api/diag?dl=1'}
let dbgMin=0;
async function toggleDebug(){const r=await api('/api/debug?min='+(dbgMin?0:120),{method:'POST'});toast(r.msg);setTimeout(status,500)}
function dbgBtn(){const b=$('dbgbtn');if(!b)return;b.textContent=dbgMin?'Diagnostic log: ON'+' ('+'remaining'+' '+dbgMin+' min)':'Diagnostic log: OFF';b.classList.toggle('p',!!dbgMin)}
async function restore(){const f=$('rsfile').files[0];if(!f)return toast('Please choose a backup file');
 let j;try{j=JSON.parse(await f.text())}catch(e){return toast('Not a valid JSON file')}
 if(j.type!=='obd2mqtt-backup')return toast('This is not an OBD2MQTT backup');
 const c=$('rscfg').checked,p=$('rsprof').checked;if(!c&&!p)return toast('Please select settings and/or profiles');
 const np=Object.keys(j.profiles||{}).length;
 if(!confirm(`${'Restore backup from'} ${j.created||'?'} (v${j.fw||'?'}) ${'?'}\n\n${c?'• '+'Settings'+(j.with_secrets?' incl. passwords':' (passwords stay as they are)')+'\n':''}${p?'• '+np+' '+'profile(s) – profiles with the same name are overwritten'+'\n':''}\n${'The device restarts afterwards.'}`))return;
 const st=$('rsst'),btn=$('rsbtn');btn.disabled=true;st.textContent='Uploading…';
 const r=await post(`/api/restore?config=${c?1:0}&profiles=${p?1:0}`,j);st.textContent=r.msg;if(!r.ok){btn.disabled=false;return}
 await new Promise(x=>setTimeout(x,5000));
 for(let i=0;i<40;i++){try{await api('/api/status');st.textContent='Done – device restarted.';toast('Backup restored');btn.disabled=false;loadCfg();prof=null;status();return}catch(e){}st.textContent='Waiting for restart…';await new Promise(x=>setTimeout(x,1500))}
 st.textContent='Device not reachable – if the WiFi changed, open the new address.';btn.disabled=false}
async function ledTest(){const r=await api('/api/ledtest?pin='+encodeURIComponent($('led_pin').value),{method:'POST'});toast(r.msg)}
async function pollNow(i){const r=await api('/api/pollnow?car='+i,{method:'POST'});toast(r.msg);setTimeout(status,3000)}
async function togglePoll(i,on){if(!on&&!confirm('Switch off automatic polling for '+carLabel(i)+'? The bridge will no longer connect to this car until you switch it on again.'))return;
 const r=await api('/api/polling?car='+i+'&on='+(on?1:0),{method:'POST'});toast(r.msg);setTimeout(status,800)}
// ---------- Jobs ----------
async function runJob(b,max=120,onProg){let r=await post('/api/job',b);if(!r.ok){toast(r.msg);return null}
 for(let i=0;i<max;i++){await new Promise(x=>setTimeout(x,400));const j=await api('/api/job');if(onProg&&j.progress)onProg(j.progress);if(j.state==='done')return j.result||{}}return {ok:false,error:'Timeout'}}
function q(c){$('tcmd').value=c;termSend()}
async function termSend(){const c=$('tcmd').value.trim();if(!c)return;const o=$('tout');o.textContent+='> '+c+'\n';
 const r=await runJob({type:'raw',cmd:c,car:selOf('tcar')});if(!r)return;o.textContent+=nl(r.ok?r.raw:'Error: '+(r.error||'')+(r.raw?'\n'+r.raw:''))+'\n\n';o.scrollTop=o.scrollHeight;$('tcmd').select()}
// ---------- Einstellungen ----------
const CF=['wifi_ssid','wifi_pass','hostname','web_pass','ip_mode','ip_addr','ip_mask','ip_gw','ip_dns1','ip_dns2','mqtt_host','mqtt_port','mqtt_user','mqtt_pass','mqtt_base','ha_prefix','min_voltage','sleep_voltage','sleep_poll_min','sleep_check_min','low_batt_voltage','charge_interval_sec','normal_interval_sec','awake_fast_polls','awake_slow_sec','dongle_retry_min','backoff_sec','cmd_timeout_ms','keep_connected','ble_tx_power','led_pin','led_brightness','ntp_server','tz','log_persist','update_check'];
const NUM=['ble_tx_power','led_pin','led_brightness','mqtt_port','min_voltage','sleep_voltage','sleep_poll_min','sleep_check_min','low_batt_voltage','charge_interval_sec','normal_interval_sec','awake_fast_polls','awake_slow_sec','dongle_retry_min','backoff_sec','cmd_timeout_ms'];
// Per-car settings: fields c0_<key> / c1_<key>
const CK=['enabled','name','device_name','mqtt_base','profile','ble_match','ble_mac','ble_pin','ble_addr_type','ble_service','ble_notify','ble_write','ble_name'];
function carForm(i,plist){const f=(k,l,a='')=>`<div><label>${l}</label><input id="c${i}_${k}" ${a}></div>`;
 return `<div style="${i?'border-top:1px solid var(--bd);margin-top:14px;padding-top:12px':''}"><h3 style="font-size:14px;margin:0 0 10px">Car ${i+1}</h3><div class="f">
  <div><label>Enabled</label><select id="c${i}_enabled"><option value="true">yes</option><option value="false">no</option></select></div>
  ${f('name','Name (web interface)')}${f('device_name','Device name in Home Assistant')}${f('mqtt_base','MQTT base topic','class="mono" placeholder="obd2mqtt/car'+(i+1)+'"')}
  <div><label>Profile</label><select id="c${i}_profile">${plist}</select></div>
  <div><label>Find dongle by</label><select id="c${i}_ble_match"><option value="mac">MAC address (fixed)</option><option value="name">Bluetooth name (MAC changes on power-up, e.g. vLinker MC+)</option></select></div>
  ${f('ble_name','Dongle Bluetooth name')}${f('ble_mac','Dongle MAC address','class="mono" placeholder="AA:BB:CC:DD:EE:FF"')}${f('ble_pin','PIN / passkey (empty = none)','inputmode="numeric" maxlength="6" placeholder="e.g. 123456 for WiCAN Pro"')}
  <div><label>Address type</label><select id="c${i}_ble_addr_type"><option value="auto">automatic</option><option value="public">public</option><option value="random">random</option></select></div>
  ${f('ble_service','Service UUID (empty = auto)','class="mono"')}${f('ble_notify','Notify UUID (empty = auto)','class="mono"')}${f('ble_write','Write UUID (empty = auto)','class="mono"')}
</div></div>`}
async function loadCfg(){const [c,pl]=await Promise.all([api('/api/config'),api('/api/profiles')]);CF.forEach(k=>{if($(k))$(k).value=String(c[k]??'')});
 pnames=pl.names||{};const cars=c.cars||[];
 $('carcfg').innerHTML=cars.map((x,i)=>{const ids=[...pl.list];if(x.profile&&!ids.includes(x.profile))ids.push(x.profile);
  return carForm(i,ids.map(n=>`<option value="${esc(n)}">${esc(pname(n))}</option>`).join(''))}).join('');
 cars.forEach((x,i)=>CK.forEach(k=>{const el=$(`c${i}_${k}`);if(el)el.value=String(x[k]??'')}));
 const sel=$('tzsel'),has=[...sel.options].some(o=>o.value===c.tz);sel.value=has?c.tz:'custom';$('tz').classList.toggle('hide',has);ipModeChange();status()}
function ipModeChange(){$('ipbox').classList.toggle('hide',$('ip_mode').value!=='static')}
async function ipTakeCurrent(){const s=await api('/api/status');if(!s||!s.wifi)return;$('ip_addr').value=s.wifi.ip||'';$('ip_mask').value=s.wifi.mask||'';$('ip_gw').value=s.wifi.gw||'';$('ip_dns1').value=s.wifi.dns||'';toast('Applied – don’t forget to save')}
const isIp=v=>/^(25[0-5]|2[0-4]\d|1?\d?\d)(\.(25[0-5]|2[0-4]\d|1?\d?\d)){3}$/.test(v);
function tzChange(){const v=$('tzsel').value;if(v==='custom'){$('tz').classList.remove('hide');$('tz').focus()}else{$('tz').value=v;$('tz').classList.add('hide')}}
async function saveCfg(rb){if($('ip_mode').value==='static'){for(const k of ['ip_addr','ip_mask','ip_gw'])if(!isIp($(k).value.trim())){toast('Invalid IP: '+$(k).previousElementSibling.textContent);$(k).focus();return}
 for(const k of ['ip_dns1','ip_dns2'])if($(k).value.trim()&&!isIp($(k).value.trim())){toast('Invalid IP: '+$(k).previousElementSibling.textContent);$(k).focus();return}}
 const b={};CF.forEach(k=>{let v=$(k).value;if(NUM.includes(k))v=Number(v);if(k==='keep_connected'||k==='log_persist'||k==='update_check')v=v==='true';b[k]=v});
 b.cars=[0,1].filter(i=>$(`c${i}_enabled`)).map(i=>{const o={};CK.forEach(k=>{o[k]=$(`c${i}_${k}`).value.trim()});o.enabled=o.enabled==='true';o.ble_mac=o.ble_mac.toUpperCase();return o});
 const on=b.cars.filter(o=>o.enabled);
 if(on.length===2&&on[0].ble_match==='mac'&&on[1].ble_match==='mac'&&on[0].ble_mac&&on[0].ble_mac===on[1].ble_mac)return toast('Both cars use the same dongle MAC');
 if(on.length===2&&on[0].ble_match==='name'&&on[1].ble_match==='name'&&on[0].ble_name&&on[0].ble_name===on[1].ble_name)return toast('Both cars find their dongle by the same name – they cannot be told apart');
 if(on.some(o=>o.ble_match==='name'&&!o.ble_name))return toast('Enter the dongle Bluetooth name (or pick it from the scan)');
 if(on.length===2&&on[0].mqtt_base===on[1].mqtt_base)return toast('Both cars use the same MQTT base topic');
 if(on.some(o=>o.ble_match==='mac'&&o.ble_mac&&!/^([0-9A-F]{2}:){5}[0-9A-F]{2}$/.test(o.ble_mac)))return toast('Invalid dongle MAC address (AA:BB:CC:DD:EE:FF)');
 const r=await post('/api/config'+(rb?'?reboot=1':''),b);toast(r.msg)}
async function scan(){const btn=$('scanbtn');btn.disabled=true;btn.textContent='Scanning (6 s)…';$('scanl').innerHTML='';
 const r=await runJob({type:'scan'});btn.disabled=false;btn.textContent='Scan for dongles';if(!r)return;
 const cn=i=>esc(($(`c${i}_name`)&&$(`c${i}_name`).value)||('Car '+(i+1)));
 $('scanl').innerHTML=(r.devices||[]).map(d=>`<div data-n="${esc(d.name||'')}"><span>${d.obd?'★ ':''}<b>${esc(d.name||'(no name)')}</b> <span class="mono">${esc(d.mac)}</span> <span class="hint" style="margin:0">${d.rssi} dBm</span></span><span class="row" style="flex-wrap:nowrap">${[0,1].map(i=>`<button class="b" onclick="pick(${i},'${esc(d.mac)}',${d.random},this.closest('div').dataset.n)">Use for ${cn(i)}</button>`).join('')}</span></div>`).join('')||'<p class="hint">No devices found.</p>'}
const bars=r=>r>=-55?'▂▄▆█':r>=-67?'▂▄▆':r>=-75?'▂▄':'▂';
async function wscan(){const btn=$('wscanbtn');btn.disabled=true;btn.textContent='Scanning…';$('wscanl').innerHTML='';let r=await api('/api/wifiscan?start=1');
 for(let i=0;i<40&&r.state!=='done';i++){await new Promise(x=>setTimeout(x,500));r=await api('/api/wifiscan')}
 btn.disabled=false;btn.textContent='Scan';const n=(r.networks||[]).sort((a,b)=>b.rssi-a.rssi);
 $('wscanl').innerHTML=n.map(w=>`<div data-s="${esc(w.ssid)}" onclick="wpick(this.dataset.s)"><span>${w.open?'':'🔒 '}<b>${esc(w.ssid)}</b> <span class="hint" style="margin:0">Channel ${w.ch}</span></span><span class="hint mono" style="margin:0">${bars(w.rssi)} ${w.rssi} dBm</span></div>`).join('')||'<p class="hint">'+'No networks found.'+'</p>'}
function wpick(s){$('wifi_ssid').value=s;$('wscanl').innerHTML='';$('wifi_pass').value='';$('wifi_pass').focus();toast('Enter password, then “Save & restart”')}
function pick(i,m,rnd,n){$(`c${i}_ble_mac`).value=m;$(`c${i}_ble_name`).value=n||'';$(`c${i}_ble_addr_type`).value=rnd?'random':'public';
 // a random address usually changes on every power-up → find this dongle by its name
 if(rnd&&n)$(`c${i}_ble_match`).value='name';
 toast('Applied to car '+(i+1)+(rnd&&n?' – random address, so it will be found by its name':'')+' – don’t forget to save')}
// ---------- Profile ----------
const DC=['','battery','voltage','current','power','energy','temperature','pressure','distance','speed','duration'];
let pnames={};const pname=id=>pnames[id]||id;
async function loadProfiles(){const r=await api('/api/profiles');active=r.active||[];pnames=r.names||{};
 const ids=[...r.list].sort((a,b)=>pname(a).localeCompare(pname(b)));
 $('psel').innerHTML=ids.map(n=>`<option value="${esc(n)}" ${n===(profName||active.find(Boolean))?'selected':''}>${esc(pname(n))}${active.includes(n)?' ✓':''}</option>`).join('');
 await loadProfile(profName&&r.list.includes(profName)?profName:(active.find(Boolean)||r.list[0]))}
async function loadProfile(n){profName=n;prof=await api('/api/profile?name='+encodeURIComponent(n));
 $('psel').value=n;renderUse();renderProfile()}
// Which enabled car uses the selected profile + "Use for <car>" buttons
function renderUse(){if(!$('usebtns'))return;const en=CARS.map((c,i)=>i).filter(i=>CARS[i].enabled);
 const users=en.filter(i=>active[i]===profName).map(carLabel);
 $('pact').textContent=users.length?'Used by: '+users.join(', '):'Not used by any car.';
 $('usebtns').innerHTML=en.filter(i=>active[i]!==profName).map(i=>`<button class="b p" onclick="activate(${i})">Use for ${esc(carLabel(i))}</button>`).join('')}
function syncAll(){const cs=[...document.querySelectorAll('#pids input[data-k=enabled]')];if($('allpids'))$('allpids').checked=cs.length>0&&cs.every(c=>c.checked)}
function batOpts(){const sel=$('p_batsel'),o=prof.battery_options||[],v=Number(prof.battery_kwh||0);
 sel.innerHTML=o.map((b,i)=>`<option value="${i}">${esc(b.name)}</option>`).join('')+`<option value="c">${'Custom value'}</option>`;
 const i=o.findIndex(b=>Math.abs(b.kwh-v)<0.05);sel.value=i>=0?String(i):'c';$('p_bat').value=v||'';$('p_cons').value=prof.consumption||'';batCalc()}
function batSel(){const v=$('p_batsel').value;if(v!=='c'){$('p_bat').value=prof.battery_options[Number(v)].kwh}else $('p_bat').focus();batCalc()}
function batInput(){const v=Number($('p_bat').value),o=prof.battery_options||[];const i=o.findIndex(b=>Math.abs(b.kwh-v)<0.05);$('p_batsel').value=i>=0?String(i):'c';batCalc()}
function batCalc(){const c=Number($('p_bat').value),k=Number($('p_cons').value);
 $('p_batinfo').textContent=c>0&&k>0?'Calculated full range: '+Math.round(c/k*100)+' km':'Enter capacity and consumption for the calculated range.'}
function renderProfile(){$('p_name').value=prof.name||'';$('p_model').value=prof.model||'';$('p_init').value=(prof.init||[]).join('\n');batOpts();
 $('pids').innerHTML=(prof.pids||[]).map((p,i)=>`<tr data-i="${i}">
 <td><input type="checkbox" data-k="enabled" ${p.enabled!==false?'checked':''}></td>
 <td><input data-k="id" value="${esc(p.id)}" style="width:90px"></td><td><input data-k="name" value="${esc(p.name)}" style="width:110px"></td>
 <td><input data-k="header" class="mono" value="${esc(p.header)}" style="width:60px"></td><td><input data-k="cmd" class="mono" value="${esc(p.cmd)}" style="width:80px"></td>
 <td><input data-k="formula" class="mono" value="${esc(p.formula)}" style="width:150px"></td><td><input data-k="unit" value="${esc(p.unit)}" style="width:50px"></td>
 <td><select data-k="device_class">${DC.map(d=>`<option value="${d}" ${d===(p.device_class||'')?'selected':''}>${d||'–'}</option>`).join('')}</select></td>
 <td><input data-k="interval" type="number" value="${p.interval??120}" style="width:70px"></td>
 <td><input data-k="min" type="number" step="any" value="${p.min??''}" style="width:60px"></td><td><input data-k="max" type="number" step="any" value="${p.max??''}" style="width:60px"></td>
 <td><input data-k="precision" type="number" min="0" max="4" value="${p.precision??1}" style="width:48px"></td>
 <td style="white-space:nowrap"><button class="b" onclick="testPid(${i})">Test</button> <button class="b d" onclick="delPid(${i})">✕</button></td></tr>`).join('');syncAll()}
function collect(){prof.name=$('p_name').value;prof.model=$('p_model').value;prof.init=$('p_init').value.split('\n').map(s=>s.trim()).filter(Boolean);
 {const c=Number($('p_bat').value),k=Number($('p_cons').value);if(c>0)prof.battery_kwh=c;else delete prof.battery_kwh;if(k>0)prof.consumption=k;else delete prof.consumption}
 document.querySelectorAll('#pids tr').forEach(tr=>{const p=prof.pids[tr.dataset.i];tr.querySelectorAll('[data-k]').forEach(el=>{const k=el.dataset.k;
  if(el.type==='checkbox')p[k]=el.checked;else if(el.type==='number'){if(el.value==='')delete p[k];else p[k]=Number(el.value)}else p[k]=el.value.trim()})})}
function addPid(){collect();prof.pids=prof.pids||[];prof.pids.push({id:'new'+(prof.pids.length+1),name:'New',header:'7E4',cmd:'220101',formula:'B3',unit:'',interval:120,precision:1,enabled:false});renderProfile()}
function toggleAll(on){document.querySelectorAll('#pids input[data-k=enabled]').forEach(c=>c.checked=on);toast(on?'All values enabled – don’t forget to save the profile':'All values disabled – don’t forget to save the profile')}
function delPid(i){collect();prof.pids.splice(i,1);renderProfile()}
async function saveProfile(n){if(!n)collect();const r=await post('/api/profile?name='+encodeURIComponent(n||profName),prof);toast(r.msg);if(r.ok){if(n)profName=n;loadProfiles()}}
function saveAs(){collect();const nm=prompt('Name of the copy:',(prof.name||profName)+' ('+'copy'+')');if(!nm)return;
 let id=nm.toLowerCase().normalize('NFD').replace(/[\u0300-\u036f]/g,'').replace(/[^a-z0-9]+/g,'_').replace(/^_+|_+$/g,'').slice(0,20)||'profil';
 const base=id;let k=2;while(pnames[id]!==undefined||id===profName){id=(base.slice(0,17)+'_'+k++)}
 prof.name=nm;pnames[id]=nm;saveProfile(id)}
async function activate(i){const cars=[{},{}];cars[i]={profile:profName};const r=await post('/api/config',{cars});toast(r.ok?'Profile '+pname(profName)+' now used for '+carLabel(i):r.msg);if(r.ok){active[i]=profName;renderUse();loadProfiles()}}
async function delProfile(){if(!confirm('Profile'+' “'+pname(profName)+'” – delete?'))return;const r=await api('/api/profile-delete?name='+encodeURIComponent(profName),{method:'POST'});toast(r.msg);if(r.ok){profName='';loadProfiles()}}
function toggleRaw(){collect();$('rawbox').classList.toggle('hide');$('raw').value=JSON.stringify(prof,null,2)}
function rawApply(){try{prof=JSON.parse($('raw').value);renderProfile();toast('Applied – now save')}catch(e){toast('JSON error: '+e.message)}}
async function testPid(i){collect();const p=prof.pids[i];$('testcard').classList.remove('hide');$('testid').textContent=p.id;$('testout').innerHTML='<p class="hint">'+'Polling…'+'</p>';
 const r=await runJob({type:'test',car:selOf('pcar'),header:p.header,cmd:p.cmd,formula:p.formula,cap:Number($('p_bat').value)||0,cons:Number($('p_cons').value)||0});if(!r){$('testout').innerHTML='';return}
 let h='';if(r.value!==undefined)h+=`<div class="val" style="max-width:260px;margin-bottom:10px"><div class="l">${esc(p.formula)}</div><div class="v">${Number(r.value).toFixed(p.precision??1)}<small>${esc(p.unit)}</small></div></div>`;
 if(!r.ok)h+=`<p style="color:var(--err)">${esc(r.error)}</p>`;
 if(r.bytes){h+=`<label>${r.count} Bytes</label><div class="bytes mono">`+r.bytes.split(' ').map((x,j)=>`<span><b>B${j}</b>${x}<b>${parseInt(x,16)}</b></span>`).join('')+'</div>'}
 h+=`<label style="margin-top:10px">${'Raw response'}</label><pre>${esc(nl(r.raw||''))}</pre>`;$('testout').innerHTML=h;$('testcard').scrollIntoView({behavior:'smooth',block:'nearest'})}
// ---------- OTA ----------
const kb=b=>(b/1024).toFixed(0)+' KB';
async function ouShow(){const u=await api('/api/online-update');if(!u||!u.state)return u;
 const st=$('oust'),ins=$('ouinst'),bar=$('oubar'),nt=$('ounotes'),ln=$('oulink');
 const chk=u.checked?` (${'checked'} ${u.checked})`:'';
 const M={idle:'Not checked yet'+(u.auto?' – '+'automatic check 3 min after start, then daily':''),checking:'Checking GitHub…',uptodate:'Firmware is up to date'+`: v${u.installed}`+chk,
  available:`${'New version available'}: ${u.latest} (${'installed'}: v${u.installed})`+chk,error:'Error: '+u.error,downloading:'Downloading and writing to flash…',done:'Done – restarting…'};
 st.textContent=M[u.state]||u.state;
 ins.classList.toggle('hide',u.state!=='available');
 ln.classList.toggle('hide',!u.url);if(u.url)ln.href=u.url;
 nt.classList.toggle('hide',!u.notes);nt.textContent=u.notes||'';
 bar.classList.toggle('hide',u.state!=='downloading');if(u.total)bar.value=Math.round(u.written/u.total*100);
 return u}
async function ouCheck(){$('oucheck').disabled=true;await api('/api/online-update?action=check',{method:'POST'});
 for(let i=0;i<40;i++){await new Promise(r=>setTimeout(r,1000));let u;try{u=await ouShow()}catch(e){continue}if(u&&u.state!=='checking'&&u.state!=='idle')break}
 $('oucheck').disabled=false}
async function ouInstall(){
 if(!confirm('Install the new firmware from the internet now? The bridge restarts afterwards.'))return;
 const r=await api('/api/online-update?action=install',{method:'POST'});if(!r.ok)return toast(r.msg);
 $('ouinst').disabled=true;$('oucheck').disabled=true;
 for(let i=0;i<200;i++){await new Promise(r=>setTimeout(r,1500));let u=null;try{u=await ouShow()}catch(e){}
  if(u&&u.state==='error'){$('ouinst').disabled=false;$('oucheck').disabled=false;return}
  if(!u){$('oust').textContent='Waiting for restart…';try{const s=await api('/api/status');if(s.fw){toast('New version running: v'+s.fw);$('ouinst').disabled=false;$('oucheck').disabled=false;status();ouShow();return}}catch(e){}}}
 $('oust').textContent='ESP32 not reachable – please reload the page'}
function ota(){const f=$('fw').files[0];if(!f)return toast('Please choose a firmware file');
 if(/factory/i.test(f.name)&&!confirm(f.name+' is the factory.bin (USB only).\nUse the *-ota.bin for updates. Upload anyway?'))return;
 const oldFw=($('fwcur').textContent.match(/v([\d.]+)/)||[])[1]||'';
 const st=$('otast'),bar=$('otabar'),btn=$('otabtn');btn.disabled=true;bar.classList.remove('hide');bar.value=0;st.textContent='Starting…';
 let done=false;
 const poll=async()=>{while(!done){try{const u=await api('/api/update');if(u.state==='running'&&u.total){const p=Math.min(99,Math.round(u.written/u.total*100));bar.value=p;st.textContent=`${'Writing to flash'}: ${p} % (${kb(u.written)} / ${kb(u.total)})`}}catch(e){}await new Promise(r=>setTimeout(r,400))}};
 const fd=new FormData();fd.append('firmware',f,f.name);const x=new XMLHttpRequest();x.open('POST','/api/update');
 x.upload.onprogress=e=>{if(bar.value<5)st.textContent='Sending file…'};
 x.onload=async()=>{done=true;let r={};try{r=JSON.parse(x.responseText)}catch(e){r={ok:false,msg:x.statusText||'Error'}}
  if(!r.ok){st.textContent=r.msg;bar.classList.add('hide');btn.disabled=false;return}
  bar.value=100;st.textContent='Done – restarting…';await new Promise(r=>setTimeout(r,4000));
  for(let i=0;i<40;i++){try{const s=await api('/api/status');st.textContent=`${'Update finished'}: v${oldFw} → v${s.fw}`;toast('New version running: v'+s.fw);btn.disabled=false;status();return}catch(e){}st.textContent='Waiting for restart…';await new Promise(r=>setTimeout(r,1500))}
  st.textContent='ESP32 not reachable – please reload the page';btn.disabled=false};
 x.onerror=()=>{done=true;st.textContent='Connection lost';btn.disabled=false};
 x.send(fd);poll()}
api('/api/config').then(c=>{if(!c.wifi_ssid)show('cfg')});
const CHANGELOG=[{"v": "0.7.9", "en": ["Manual (Help): new section “Status LED” with all colours, and an updated “How often is polled?” (charging / awake / asleep / dongle not found intervals); old language-switch row removed", "Settings → Status LED: colour legend instead of the long hint text"]}, {"v": "0.7.8", "en": ["One button instead of two: “Check now” is merged into “Poll now” (Status card and Home Assistant). In HA the existing button keeps its entity id and is now named “Poll now” – automations keep working. Works even with the Polling switch off"]}, {"v": "0.7.7", "en": ["“Poll now” no longer wakes a sleeping car: below the sleep voltage only the 12 V is checked (log: “car is asleep … not woken”); an awake car is polled in full as before"]}, {"v": "0.7.6", "en": ["Car card: the next poll / 12V check is shown on its own line under the OBD line, so it fits on phones"]}, {"v": "0.7.5", "en": ["Awake but not charging: 3 polls every 5 min, then every 30 min (settings “Awake, not charging: polls at that interval, then slower” and “… then poll every”). Charging starting or stopping, the car waking up, Check now and Poll now start the count again; the log notes the switch"]}, {"v": "0.7.4", "en": ["Car card shows when the car is served next: “next poll 20:05 (in 4 min 12 s) · every 5 min”, “every 1 min (charging)”, “next 12V check …” while asleep, “next attempt …” when the dongle was not found, “retry 1/2 …” after a failed setup, or “polling off” – counts down every second", "New Home Assistant sensor “Next poll” per car (timestamp)"]}, {"v": "0.7.3", "en": ["“Subscribe failed” (dongle connected but refused the notification setup): retried after 60 s, up to 2 times, instead of waiting 30 min – the dongle is clearly in range. The log now shows the signal strength (RSSI) with this error", "MQTT keepalive 60 s (was 30 s): a hanging Bluetooth step (up to 30 s) no longer makes the broker drop the connection"]}, {"v": "0.7.2", "en": ["New poll intervals in Settings → Polling, for both cars: every 60 s while charging (AC or DC, from the PIDs “charging” / “charging_dc”), every 300 s when awake and not charging. PIDs with a longer profile interval (odometer, SOH) keep theirs; 0 = use the profile intervals", "Dongle not found: next attempt after 30 min (setting, replaces “Reconnect attempt after” 60 s). “Check now” in Home Assistant still connects immediately", "Log shows when a car starts or stops charging and the interval used"]}, {"v": "0.7.1", "en": ["Status LED (RGB LED on ESP32-S3/C3 boards): blue blinking = connecting, green = polling (flash per reply), dim green = idle, short blue pulse = car asleep, yellow = dongle not found / no data, orange = MQTT down, white / purple blinking = no WiFi / setup hotspot, cyan = firmware update, red blinking = safe mode", "Settings → Status LED: GPIO (default 48 on the S3, 8 on the C3, none on the classic ESP32), brightness, and a Test button that lights a GPIO red, green, blue to find the board's LED"]}, {"v": "0.7.0", "en": ["New web interface layout, made for phones: tab bar at the bottom on phones, at the top on wide screens", "Status: the live log comes first (car names in colour, errors in red, “Full log” for more lines); each car card shows its state (Asleep / Awake / AC or DC charging), last data and three tiles – 12V, SoC, Charging – with “Show all values” for the rest; OBD connection, Check now, Poll now and the Polling switch at the bottom of the card", "Diagnostics moved to the System tab; the car dropdown in the header is gone – Terminal, Test and Diagnostics each have their own car choice", "ESP32-S3 support: own firmware files (…-esp32s3-ota.bin / -factory.bin); the build now also creates a factory.bin for the first USB flash", "Settings → Bluetooth transmit power (ESP32: up to +9 dBm, ESP32-S3: up to +20 dBm) for more range", "Firmware upload refuses files built for another chip; the online update only installs files for this board"]}, {"v": "0.6.3", "en": ["While the car is asleep the 12 V is checked every 30 min instead of every minute (setting “Check 12V while car is asleep every”)", "New Home Assistant button “Check now” per car (and on the Status card): checks the 12 V right away; an awake car (e.g. AC charging) is polled immediately, a sleeping car is not woken. Use it in an automation, e.g. when the charger starts", "“Poll now” is unchanged: it wakes the car for one full poll"]}, {"v": "0.6.2", "en": ["New Home Assistant sensor “Last data” per car: time of the last poll that delivered values. Bridge restarts no longer look like fresh data", "Status card shows “last data: … ago” per car", "If the first query of a cycle answers NO DATA (car asleep), the other queries (odometer, tyres …) are skipped, so they don't keep the car awake"]}, {"v": "0.6.0", "en": ["New: find the dongle by its Bluetooth name (Settings → Cars → Find dongle by) – for dongles whose MAC changes on every power-up, e.g. vLinker MC+. The bridge tries the last address first and only scans (approx. 4 s) when the dongle has a new address", "Dongle scan: picking a dongle with a random address switches that car to “find by name” automatically", "Two cars set to find their dongle by the same name are rejected when saving (they could not be told apart)"]}, {"v": "0.5.0", "en": ["New: two cars – one dongle per car, each with its own profile, MQTT base topic (default obd2mqtt/car1, obd2mqtt/car2) and Home Assistant device. One BLE connection at a time; the bridge serves whichever car is due", "Settings → Cars: name, HA device name, base topic, profile and dongle per car; dongle scan with \"Use for …\" per car", "Status: one card per car with its own polling switch and Poll now; car selector (top right) for Test, Terminal and diagnostic report", "Profile: \"Use for …\" a car instead of Activate; new factory profile Kia EV6 (same E-GMP PIDs as the IONIQ 5)", "Home Assistant: new \"OBD2MQTT Bridge\" device with the firmware update; polling switch per car", "Upgrade from 0.4: existing dongle/profile settings become car 1; topics move to obd2mqtt/car1 – delete the old device in Home Assistant once", "Firmware upload switches Bluetooth off completely while flashing (as in 0.4.1)"]}, {"v": "0.4.1", "en": ["Firmware upload (System → Firmware update) switches Bluetooth off completely while flashing – lower current, against brownout restarts during the upload. Interim version to install 0.5"]}, {"v": "0.4.0", "en": ["English only: web interface, manual, log messages, Home Assistant names and factory profile; language switch removed. Manual now at /help (/hilfe redirects)", "Only the Hyundai IONIQ 5 / Kia EV6 profile is included (XPeng profiles removed); startup repair of old profiles removed", "New: tyre pressure (bar) and temperature for all four tyres (TPMS 7A0 / 22C00B)", "Faster web interface: pages sent gzip-compressed and cached (304), log only fetched when it changes, lighter status polling, polling paused in hidden browser tabs", "Test and Terminal now run between values of a poll cycle instead of waiting for the whole cycle", "Less load in the main loop: BLE signal read every 2 s, file system usage cached, log written to flash in batches", "Online update checks github.com/anas-ivs/obd2mqtt; default time zone Malaysia (MYT-8)"]}, {"v": "0.3.23", "en": ["New: online update (System → Online update) – checks the latest release on GitHub, shows the changes since the installed version and installs the *-esp32-ota.bin directly from the internet at the push of a button (encrypted connection with certificate check, size and image check, then restart)", "Automatic check 3 min after start and then daily (can be switched off in the settings); a new version is noted in the log", "Home Assistant: new update entity “Firmware” – shows installed and latest version, “Install” starts the online update", "Fix: “Download backup” produced an empty file (0 bytes) – the backup was built completely in RAM (JSON document, text and another copy for the response, together ~3 × 30 KB), which failed when memory was tight. It is now sent in pieces: settings, then every profile directly from its file", "Restore needs less memory: the uploaded file is parsed directly in the receive buffer instead of being copied twice"]}, {"v": "0.3.22", "en": ["WiFi: after a disconnect the bridge reconnects by itself every 30 s – the ESP32's automatic reconnect got stuck on some disconnect reasons until the watchdog restarted after 15 min", "WiFi: disconnect reason in the log (e.g. “reason 200: beacon timeout (weak signal / interference)”) with the last signal value; the signal strength is shown when connecting", "Time: implausible NTP times (before the firmware build or more than 5 years after it, e.g. 2034) are discarded and requested again; until a valid time is available the log shows the uptime"]}, {"v": "0.3.21", "en": ["Watchdog: after a hang the log shows where the firmware hung (e.g. “Firmware hung at: BLE subscribe/pairing”) – also in the diagnostic report", "BLE connection setup hardened: the watchdog is fed before every step, setup is aborted after 20 s and retried later (previously a hanging connection setup or PIN pairing could trigger the 60 s watchdog); the device name is only read on the first connection", "If the connection drops during initialisation, it is aborted immediately (instead of reporting every init line as “without prompt”)", "If the connection drops during polling, there is no 10 min pause (“No valid response”) any more – retry after 60 s; error “BLE disconnected” instead of “ATSH… failed”", "New: if the connection drops repeatedly right after connecting (reason 0x208), the bridge automatically uses slower BLE connection parameters (30–50 ms instead of 7.5–15 ms) – shown in the log, diagnostic report and status", "Warm-up: after connecting, a short “Tester Present” (3E00) is sent to the ECU before the first query – against the truncated first long response (only when the car is awake)", "Cleaner log: no [diag] init lines during the minute-by-minute voltage check while asleep; “BLE pairing OK” only the first time; “Dongle not reachable” while the car sleeps only once (dongle sleeps with the car), plus “Dongle reachable again”", "RSSI: invalid readings right after connecting (e.g. −128 dBm) are shown as “not measured yet”", "Diagnostic report: up to 3 connection attempts, clean abort with a note on connection loss, restart reason, BLE parameters, drops and RSSI at the end", "Fix: when scanning again the dongle name was missing (MAC only) – the name from the scan response is now remembered; for the configured dongle the known name is shown as a fallback", "Formulas: the last value of any other value is available by its ID (e.g. hv_voltage) – for quantities from two responses", "XPeng G9/G6/P7+: new values charging power (kW), charging and DC charging from HV current × HV voltage (first G9 test: 713 V × −10.5 A ≈ 7.5 kW while AC charging); “Charging status” is now called “raw value” (the G9 returned 1 while AC charging); existing XPeng profiles are extended automatically"]}, {"v": "0.3.20", "en": ["Fix: “Last error” in MQTT still stayed – the old value is retained in the broker and was never overwritten after a restart; also the reset waited for rarely polled values. Now: “none” after every error-free poll cycle, and the current state is sent immediately when MQTT connects"]}, {"v": "0.3.19", "en": ["Fix: “Last error” was never reset – as soon as all active values are read without errors again it is cleared (status page empty, Home Assistant “none”)"]}, {"v": "0.3.18", "en": ["Charging values set to 0 when the car falls asleep: if the car fell asleep right after charging, Home Assistant kept showing the last charging power – charging power, charging, DC charging, HV current (XPeng: charge status, HV current) are now set to 0 when the car falls asleep", "Profiles: new per-value field “sleep_zero” (in the JSON editor) – set the value to 0 when the car falls asleep; existing factory-derived profiles are extended automatically", "Note: the charging power is the power arriving in the battery (HV voltage × HV current) – measured at the wallbox it is about 10 % higher when AC charging (test: 3.7 kW in the battery at 4.1 kW at the wallbox)"]}, {"v": "0.3.17", "en": ["IONIQ 5: “Charging” and “DC charging” are now detected from the HV current (the charging bits in 220101 byte 12 always stay 0 on the IONIQ 5) – confirmed during AC charging: −7.0 A at 527.5 V = 3.7 kW", "IONIQ 5: new values HV current, HV voltage and charging power (kW); existing IONIQ profiles are extended automatically", "Formulas: new functions lt(a,b), gt(a,b), min(a,b), max(a,b)"]}, {"v": "0.3.16", "en": ["New: dongles with pairing PIN (e.g. WiCAN Pro via Bluetooth) – Settings → BLE dongle → “PIN / passkey”; the bridge pairs automatically (encrypted connection, stored bond), messages in the log; with a wrong PIN the bond is discarded", "Manual: section “Dongle with PIN (e.g. WiCAN Pro)”"]}, {"v": "0.3.15", "en": ["New: Profile → “Battery & range” – pick the battery variant from a list (known sizes pre-filled: IONIQ 5 58–84 kWh, G9, G6, P7+), capacity can be corrected by hand, consumption in kWh/100 km, calculated full range shown", "Formulas can use the variables CAP (usable kWh) and CONS (kWh/100 km); the range formulas of the factory profiles use them (“Test” uses the values from the form)", "Stored profiles are converted: old range formula “…/100*kWh/consumption” → CAP/CONS with the values taken over, battery selection list added", "IONIQ 5 factory profile: ATAT0 (no adaptive timing) and explicit flow control (ATFCSH7E4, ATFCSD300000, ATFCSM1) – long responses like 220101 almost never arrived complete with an “OBDII v1.5” clone, now 6 of 7 in a test; unmodified IONIQ profiles are converted automatically", "With user-defined flow control (ATFCSM1) the flow-control address follows every header change automatically (e.g. 7E4 → 7C6 for the odometer)", "MQTT: battery capacity and consumption in the profile attributes"]}, {"v": "0.3.14", "en": ["Fix: in sleep mode “BLE disconnected (reason 0x216)” was still logged every minute – filled the log in about 4 hours and pushed out older entries"]}, {"v": "0.3.13", "en": ["Lower memory use: log download and diagnostic report are sent in 1 KB chunks instead of being built completely in RAM – previously “Download/Copy” briefly needed up to ~40 KB of heap (measured minimum 25 KB)", "Diagnostic report is fetched via /api/diag (no longer inside the job JSON)"]}, {"v": "0.3.12", "en": ["New: diagnostic report (Status → Diagnostics) – fresh connection, every init response (rejected commands marked), ATI/AT@1/ATRV/ATDPN, every query of the profile with raw response, response time, indexed bytes and calculated values, summary; copy/download; without WiFi, IP and password data (dongle MAC shortened)", "New: diagnostic log for 2 hours – every request with raw response, init responses, voltage/protocol/MTU on each connection; switches itself off", "Init commands rejected by the dongle (“?”) are now always logged", "Manual: guide “Diagnostic report for beta testers”"]}, {"v": "0.3.11", "en": ["XPeng G6/G9/P7+: init sequence fixed – ATFCSD300000 added before ATFCSM1 (otherwise the ELM327 rejects the user-defined flow control and multi-frame responses never arrive; bug from the WiCAN profile), plus ATAL", "Stored profiles are repaired automatically at startup (missing ATFCSD added, 7DF queries disabled when a fixed receive filter ATCRA is set) – noted in the log", "XPeng factory profiles: “SoC (OBD2 standard)” removed – it could never answer because of the receive filter on 784", "Sleep mode with hysteresis like WiCAN: sleep below the threshold, awake only from threshold + 0.1 V", "Polling while asleep (every N min) only above 11.9 V", "New: 12V warning – Home Assistant sensor “12V low” (<base>/battery_low) below an adjustable threshold (default 12.0 V)"]}, {"v": "0.3.10", "en": ["BLE: larger packets (MTU 247) and shorter connection interval – more throughput against “Incomplete response” with long responses and weak signal; MTU is shown in the log", "Sleep mode: the check every minute no longer writes “BLE connected/disconnected” to the log each time (fewer flash writes)", "IONIQ 5 factory profile: “AC plug” removed – stays 0 even with the charging cable plugged in; existing profiles: disable or delete the value"]}, {"v": "0.3.9", "en": ["IONIQ 5 factory profile: “Charging BMS” (220106) removed – the bit is 1 whenever the car is awake (12 V top-up), even without a charging cable; existing profiles: disable or delete the value", "Sleep mode: if the 12 V voltage cannot be read, a sleeping car is left alone (previously it was polled and woken up)", "Log shows when “Poll now” wakes a sleeping car"]}, {"v": "0.3.8", "en": ["New: main switch “Polling ON/OFF” – on the status page and in Home Assistant as a switch (<base>/polling, command to <base>/polling/set). When OFF the bridge no longer connects to the car; Test, terminal and “Poll now” keep working. The setting survives a restart", "New: let the car sleep – if the 12 V voltage is below 13.2 V (car off, not charging), no requests are sent to the car, only the voltage at the dongle is measured. Previously polling every 2 min kept waking the IONIQ 5 (12 V jumped to 14.7 V each time)", "Settings → Polling: threshold “Car asleep below” and optional “Poll while car is asleep” (every N min); “Poll now” always polls", "Status shows “Car awake/asleep” with voltage; Home Assistant: new sensor “Car awake” (<base>/car_awake)", "Fix: the 10 min pause “No valid response” also triggered on incomplete responses although the car was awake – now only when the car does not respond at all", "IONIQ 5 factory profile: longer response timeout (ATSTFF) against incomplete multi-frame responses (existing profiles: change init command ATST96 → ATSTFF or restore factory profiles)", "Less CAN traffic: identical requests are sent only once per poll cycle (previously e.g. 220105 up to 3× per cycle)", "IONIQ 5 factory profile: new experimental values “Charging”, “DC charging”, “AC plug” (220101 byte 12, per WiCAN) and “Charging BMS” (220106 byte 27, per evDash) – please verify with Test"]}, {"v": "0.3.7", "en": ["Settings → WiFi: IP address automatic (DHCP) or static (IP, subnet mask, gateway, DNS); button “Use current values”", "Fallback: if the static IP does not work at startup (no WiFi connection or MQTT broker not reachable), the bridge automatically gets an address via DHCP – shown in the log, under System and in the settings", "Invalid entries (e.g. gateway not in subnet) are detected when saving or at startup"]}, {"v": "0.3.6", "en": ["Fix: crash loop caused by a full file system – the log files could use ~96 KB of 128 KB; now max. 2 × 16 KB, and the log never uses more than 70 % of the flash (profiles/settings take priority)", "Fix: log files are only read/written under a lock – downloading could collide with simultaneous writing/rotation", "Self-healing: after 2 crashes in a row the older log is deleted, after 3 the current one too; same at startup when the file system is too full; old 48 KB logs are removed on update", "System: free heap with minimum and largest block, plus file system usage", "Status in safe mode shows the MQTT connection correctly again", "Log lines right after startup use local time instead of UTC"]}, {"v": "0.3.5", "en": ["MQTT: active vehicle profile at <base>/profile (+ details as JSON at <base>/profile/attributes), in Home Assistant as sensor “Vehicle profile”", "MQTT: all values including profile and timestamp as one JSON at <base>/state after each poll cycle"]}, {"v": "0.3.4", "en": ["Time zone as an understandable selection list (e.g. “Central Europe – Germany, Austria, Switzerland …”) instead of a POSIX string; a custom value is still possible", "Current time on the device is shown below the time zone", "Dongle is shown with its name: Bluetooth name, detected type and ELM version (e.g. “vLinker MC · vLinker / iOS-Vlink · ELM327 v2.2”), also in the status and in the log"]}, {"v": "0.3.3", "en": ["Profile selection shows readable names instead of file names (e.g. “Hyundai IONIQ 5 / Kia EV6”, “XPeng G9”), sorted, active profile marked with ✓", "“Save copy” only asks for a name – the internal file name is generated automatically", "Factory profiles renamed: “Hyundai IONIQ 5 / Kia EV6”, “XPeng P7+ (experimental)”"]}, {"v": "0.3.2", "en": ["Factory profiles: all values enabled by default (IONIQ 5, G9, G6, P7+)", "Profile table: checkbox in the “Active” column header switches all values on/off"]}, {"v": "0.3.1", "en": ["New factory profiles: XPeng G6 and XPeng P7+ (P7+ experimental)", "Existing devices get the new profiles automatically after the update"]}, {"v": "0.3.0", "en": ["Complete interface, device messages and manual in German and English – language selector at the top right", "Language is stored on the device; log, error messages and Home Assistant names follow the selection", "Factory profiles with German or English value names", "Bilingual changelog (CHANGELOG.md / CHANGELOG.en.md)"]}, {"v": "0.2.5", "en": ["Back up settings and profiles (JSON file, optionally including passwords)", "Restore a backup – settings and/or profiles, followed by an automatic restart"]}, {"v": "0.2.4", "en": ["User manual built into the device (link “Help” at the top right or /hilfe)", "Manual with search box and live check of your own system with links to the matching help"]}, {"v": "0.2.3", "en": ["Log with real time of day (NTP, configurable time zone; before sync the uptime “+hh:mm:ss”)", "Store log permanently in flash (survives restarts, rotates at 48 KB), can be disabled", "Log: copy, download, download older log, clear", "Changelog in the System tab"]}, {"v": "0.2.2", "en": ["Warm-up after dongle reset (ATRV + pause) – fixes CAN ERROR on the Test button", "Single retry on CAN ERROR", "BLE stays connected for 60 s after Test/Terminal"]}, {"v": "0.2.1", "en": ["Late ELM prompts (“>”) are ignored – fixes truncated responses", "Wait until the dongle is quiet before each command", "Up to 2 retries on incomplete responses, raw response in the log"]}, {"v": "0.2.0", "en": ["Watchdog: hang > 60 s, WiFi 15 min or MQTT 20 min down → restart", "Safe mode after 3 crashes in a row (no BLE, web UI reachable)", "Reason of the last restart in the System tab"]}, {"v": "0.1.10", "en": ["Fix: “Restore factory profiles” and “Delete profile” reported “Invalid profile name”"]}, {"v": "0.1.9", "en": ["Incomplete multi-frame responses are detected and retried", "IONIQ 5: init command ATST96 (longer timeout)", "Clearer log message for scheduled BLE disconnects"]}, {"v": "0.1.8", "en": ["Status LED removed (ESP32-DevKitC V4 has no controllable LED)"]}, {"v": "0.1.7", "en": ["Firmware update with real progress, error messages and waiting for the restart", "Check for factory.bin, BLE polling pauses during the update"]}, {"v": "0.1.5 – 0.1.6", "en": ["Version number in the file name and in the web interface", "LED test (removed later)"]}, {"v": "0.1.4", "en": ["XPeng G9: SoH 22110A, odometer 3 bytes, CLTC range 221118, charge limit/charge status"]}, {"v": "0.1.3", "en": ["Battery temperature, odometer, calculated range; G9 profile based on WiCAN"]}, {"v": "0.1.1", "en": ["WiFi scan in the settings"]}, {"v": "0.1.0", "en": ["First version: BLE ELM327 → MQTT with Home Assistant discovery, web interface, profiles, test, terminal, OTA"]}];
function renderChangelog(){$('changelog').innerHTML=CHANGELOG.map((c,i)=>`<div style="margin-bottom:10px"><b>v${esc(c.v)}</b>${i==0?' <span class="pill" style="padding:0 7px;font-size:11px">new</span>':''}<ul style="margin:4px 0 0 18px;padding:0">${c.en.map(x=>`<li>${esc(x)}</li>`).join('')}</ul></div>`).join('')}
renderChangelog();
// skip polling while the browser tab is hidden – no load on the ESP32 from background tabs
status();setInterval(()=>{if(!document.hidden&&(cur==='status'||cur==='sys'||cur==='cfg'))status()},3000);
document.addEventListener('visibilitychange',()=>{if(!document.hidden)status()});
</script></body></html>)HTML";
