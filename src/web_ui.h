#pragma once
#include <Arduino.h>

static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>OBD2MQTT</title>
<style>
:root{--bg:#f4f5f7;--card:#fff;--fg:#1d2330;--mut:#6b7385;--bd:#dfe3ea;--acc:#1f6feb;--ok:#1a7f37;--err:#cf222e;--warn:#9a6700;--in:#fff}
@media(prefers-color-scheme:dark){:root{--bg:#0f1218;--card:#181c24;--fg:#e6e9ef;--mut:#8b93a5;--bd:#2a303c;--acc:#4c8dff;--ok:#3fb950;--err:#f85149;--warn:#d29922;--in:#11151c}}
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
</style></head><body>
<header><div><h1>OBD2MQTT</h1><div class="sub" id="hsub">BLE-OBD2 → MQTT bridge</div></div>
<nav id="nav"><button data-t="status" class="on">Status</button><button data-t="profile">Profile</button><button data-t="term">Terminal</button><button data-t="cfg">Settings</button><button data-t="sys">System</button><a id="helplink" href="/help" target="_blank" style="color:var(--acc);text-decoration:none;padding:7px 11px">Help ↗</a></nav></header>
<main>

<section id="t-status">
 <div class="card hide" id="safebox" style="border-color:var(--err)"><h2 style="color:var(--err)">Safe mode</h2>
  <p style="margin:0">The firmware crashed several times in a row. Bluetooth and polling are therefore disabled, the web interface keeps running. Check profile and settings, then restart normally.</p>
  <div class="row" style="margin-top:10px"><button class="b p" onclick="api('/api/reboot',{method:'POST'}).then(r=>toast(r.msg))">Restart normally</button></div></div>
 <div class="card"><div class="row" style="justify-content:space-between"><h2 style="margin:0">Values</h2><div class="row" style="margin:0"><button class="b" id="pollsw" onclick="togglePoll()">Polling: ON</button><button class="b" onclick="pollNow()">Poll now</button></div></div>
  <div class="grid" id="vals" style="margin-top:12px"></div></div>
 <div class="card"><h2>Connection</h2><div class="row" id="conn"></div><p class="hint" id="lerr"></p></div>
 <div class="card"><div class="row" style="justify-content:space-between"><h2 style="margin:0">Diagnostics</h2>
  <div class="row"><button class="b" id="dbgbtn" onclick="toggleDebug()">Diagnostic log: OFF</button><button class="b p" id="diagbtn" onclick="runDiag()">Create diagnostic report</button></div></div>
  <p class="hint">The <b>diagnostic report</b> reconnects to the dongle, records every init response and queries every value of the profile once with raw data (takes up to approx. 1 min, wakes the car). It contains no WiFi names, IP addresses or passwords and can therefore be posted e.g. in the forum. The <b>diagnostic log</b> additionally writes every request with its raw response to the log for 2 hours.</p>
  <div id="diagbox" class="hide" style="margin-top:10px"><div class="row"><button class="b" onclick="copyDiag()">Copy</button><button class="b" onclick="dlDiag()">Download</button></div>
  <pre id="diagout" style="max-height:400px;margin-top:10px"></pre></div></div>
 <div class="card"><div class="row" style="justify-content:space-between"><h2 style="margin:0">Log</h2>
  <div class="row"><button class="b" onclick="copyLog()">Copy</button><a class="b" style="text-decoration:none" href="/api/log?dl=1">Download</a><a class="b" style="text-decoration:none" href="/api/log?old=1&amp;dl=1">Older log</a><button class="b d" onclick="if(confirm('Delete log?'))api('/api/log-clear',{method:'POST'}).then(r=>{toast(r.msg);status()})">Delete</button></div></div>
  <pre id="log" style="max-height:300px;margin-top:12px"></pre>
  <p class="hint">Showing the last 60 lines. “Copy” and “Download” return the complete log – across restarts if log storage is enabled.</p></div>
</section>

<section id="t-profile" class="hide">
 <div class="card"><div class="row">
  <div style="flex:1;min-width:160px"><label>Profile</label><select id="psel" onchange="loadProfile(this.value)"></select></div>
  <div style="align-self:end" class="row"><button class="b p" onclick="activate()">Activate</button><button class="b" onclick="saveAs()">Save copy…</button><button class="b d" onclick="delProfile()">Delete</button></div></div>
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
  <div class="row"><input id="tcmd" class="mono" style="flex:1;min-width:180px" placeholder="e.g. ATRV, ATSH7E4, 220105" onkeydown="if(event.key==='Enter')termSend()"><button class="b p" onclick="termSend()">Send</button></div>
  <div class="row" style="margin-top:8px"><span class="hint" style="margin:0">Quick:</span>
   <button class="b" onclick="q('ATI')">ATI</button><button class="b" onclick="q('ATRV')">ATRV</button><button class="b" onclick="q('ATDP')">ATDP</button><button class="b" onclick="q('0100')">0100</button><button class="b" onclick="q('ATSH7E4')">ATSH7E4</button><button class="b" onclick="q('220105')">220105</button></div>
  <pre id="tout" style="margin-top:12px;min-height:200px"></pre>
  <p class="hint">Commands run between the automatic polls. After ATSH commands the poller sets the header again by itself.</p></div>
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
 <div class="card"><h2>BLE-Dongle</h2><div class="f">
  <div><label>MAC address</label><input id="ble_mac" class="mono" placeholder="AA:BB:CC:DD:EE:FF"></div>
  <div><label>PIN / passkey (empty = none)</label><input id="ble_pin" inputmode="numeric" maxlength="6" placeholder="e.g. 123456 for WiCAN Pro"></div>
  <div><label>Address type</label><select id="ble_addr_type"><option value="auto">automatic</option><option value="public">public</option><option value="random">random</option></select></div>
  <div><label>Service UUID (empty = auto)</label><input id="ble_service" class="mono"></div>
  <div><label>Notify UUID (empty = auto)</label><input id="ble_notify" class="mono"></div>
  <div><label>Write UUID (empty = auto)</label><input id="ble_write" class="mono"></div><input type="hidden" id="ble_name"></div>
  <div class="row" style="margin-top:12px"><button class="b" id="scanbtn" onclick="scan()">Scan for dongles</button><span class="hint" id="uuids" style="margin:0"></span></div>
  <div class="scanl" id="scanl" style="margin-top:8px"></div></div>
 <div class="card"><h2>Polling</h2><div class="f">
  <div><label>12V minimum voltage (0 = off)</label><input id="min_voltage" type="number" step="0.1"></div>
  <div><label>Car asleep below (V, 0 = off)</label><input id="sleep_voltage" type="number" step="0.1"></div>
  <div><label>Poll while car is asleep (min, 0 = never)</label><input id="sleep_poll_min" type="number"></div>
  <div><label>12V warning below (V, 0 = off)</label><input id="low_batt_voltage" type="number" step="0.1"></div>
  <div><label>Reconnect attempt after (s)</label><input id="retry_sec" type="number"></div>
  <div><label>Pause when car does not respond (s, 0 = off)</label><input id="backoff_sec" type="number"></div>
  <div><label>NTP server (time)</label><input id="ntp_server" placeholder="pool.ntp.org or fritz.box"></div>
  <div><label>Time zone</label><select id="tzsel" onchange="tzChange()"><option value="MYT-8">Malaysia, Singapore</option><option value="CET-1CEST,M3.5.0,M10.5.0/3">Central Europe – Germany, Austria, Switzerland, Italy, France …</option><option value="GMT0BST,M3.5.0/1,M10.5.0">United Kingdom, Ireland</option><option value="WET0WEST,M3.5.0/1,M10.5.0">Portugal, Canary Islands</option><option value="EET-2EEST,M3.5.0/3,M10.5.0/4">Eastern Europe – Finland, Greece, Romania, Baltics …</option><option value="&lt;+03&gt;-3">Turkey</option><option value="MSK-3">Moscow</option><option value="UTC0">UTC (no daylight saving)</option><option value="EST5EDT,M3.2.0,M11.1.0">USA/Canada – Eastern</option><option value="CST6CDT,M3.2.0,M11.1.0">USA/Canada – Central</option><option value="MST7MDT,M3.2.0,M11.1.0">USA/Canada – Mountain</option><option value="PST8PDT,M3.2.0,M11.1.0">USA/Canada – Pacific</option><option value="CST-8">China</option><option value="JST-9">Japan</option><option value="AEST-10AEDT,M10.1.0,M4.1.0/3">Australia – Sydney, Melbourne</option><option value="custom">Other (POSIX string) …</option></select><input id="tz" class="mono hide" style="margin-top:6px" placeholder="e.g. CET-1CEST,M3.5.0,M10.5.0/3"><p class="hint" id="devtime"></p></div>
  <div><label>Store log permanently (flash)</label><select id="log_persist"><option value="true">yes – survives restarts</option><option value="false">no – RAM only</option></select></div>
  <div><label>Check daily for new firmware (GitHub)</label><select id="update_check"><option value="true">yes</option><option value="false">nein</option></select></div>
  <div><label>Command timeout (ms)</label><input id="cmd_timeout_ms" type="number"></div>
  <div><label>Keep BLE connected permanently</label><select id="keep_connected"><option value="false">no – dongle may sleep</option><option value="true">yes</option></select></div></div></div>
 <div class="card"><h2>MQTT / Home Assistant</h2><div class="f">
  <div><label>Broker</label><input id="mqtt_host" placeholder="192.168.1.10"></div>
  <div><label>Port</label><input id="mqtt_port" type="number"></div>
  <div><label>User</label><input id="mqtt_user" autocomplete="off"></div>
  <div><label>Password</label><input id="mqtt_pass" type="password" autocomplete="new-password"></div>
  <div><label>Base topic</label><input id="mqtt_base"></div>
  <div><label>Discovery prefix</label><input id="ha_prefix"></div>
  <div><label>Device name in HA</label><input id="device_name"></div></div></div>
 <div class="row"><button class="b p" onclick="saveCfg(false)">Save</button><button class="b" onclick="saveCfg(true)">Save &amp; restart</button>
  <span class="hint" style="margin:0">WiFi, MQTT and password changes take effect after the restart.</span></div>
</section>

<section id="t-sys" class="hide">
 <div class="card"><h2>System</h2><div id="sysinfo" class="mono"></div>
  <div class="row" style="margin-top:12px"><button class="b" onclick="api('/api/reboot',{method:'POST'}).then(r=>toast(r.msg))">Restart</button>
  <button class="b d" onclick="if(confirm('Overwrite factory profiles (ioniq5, g9, g6, p7plus)?'))api('/api/profile-defaults',{method:'POST'}).then(r=>{toast(r.msg);loadProfiles()})">Restore factory profiles</button></div></div>
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
let cur='status',prof=null,profName='',active='';
function toast(m){const t=$('toast');t.textContent=m;t.classList.add('show');clearTimeout(t._h);t._h=setTimeout(()=>t.classList.remove('show'),2600)}
async function api(u,o={}){const r=await fetch(u,o);let j;try{j=await r.json()}catch(e){j={ok:r.ok,msg:r.statusText}}if(!r.ok&&j.ok===undefined)j.ok=false;return j}
const post=(u,b)=>api(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)});
document.querySelectorAll('#nav button').forEach(b=>b.onclick=()=>show(b.dataset.t));
function show(t){cur=t;document.querySelectorAll('#nav button').forEach(b=>b.classList.toggle('on',b.dataset.t===t));
 document.querySelectorAll('main>section').forEach(s=>s.classList.toggle('hide',s.id!=='t-'+t));
 if(t==='profile'&&!prof)loadProfiles();if(t==='cfg')loadCfg();if(t==='sys'){status();ouShow().catch(()=>{})}}
const age=s=>s<0?'never':s<60?s+' s':s<3600?Math.round(s/60)+' min':Math.round(s/3600)+' h';
function pill(ok,txt){return `<span class="pill"><span class="dot ${ok?'ok':'err'}"></span>${esc(txt)}</span>`}
async function status(){try{const s=await api('/api/status');
 $('hsub').textContent='v'+s.fw+' · '+(s.profile_name||s.profile)+' · '+(s.wifi.ip||'');if($('fwcur'))$('fwcur').textContent='Installed: v'+s.fw;
 $('vals').innerHTML=s.pids.filter(p=>p.enabled).map(p=>`<div class="val"><div class="l">${esc(p.name)}</div><div class="v">${p.value??'–'}<small>${esc(p.unit)}</small></div><div class="m">${'updated'}: ${age(p.age_s)} · ${'next'}: ${p.next_s} s</div>${p.error?`<div class="e">${esc(p.error)}</div>`:''}</div>`).join('')
  +(s.voltage!==undefined?`<div class="val"><div class="l">${'12V battery'}</div><div class="v">${s.voltage.toFixed(1)}<small>V</small></div></div>`:'');
 $('conn').innerHTML=pill(s.wifi.connected,'WLAN '+(s.wifi.ssid||'')+(s.wifi.connected?' ('+s.wifi.rssi+' dBm)':''))+pill(s.mqtt,'MQTT')+(s.poll_enabled===false?pill(false,'Polling switched off'):'')+(s.voltage!=null?`<span class="pill"><span class="dot ${s.car_asleep?'':'ok'}"></span>${s.car_asleep?'Car asleep':'Car awake'} (${Number(s.voltage).toFixed(1)} V)</span>`:'')
  +pill(s.ble.connected,'BLE '+(s.ble.name?s.ble.name+(s.ble.mac?' ('+s.ble.mac+')':''):(s.ble.mac||'not configured'))+(s.ble.connected?' ('+s.ble.rssi+' dBm)':s.ble.next_try_s?' – '+'retry in'+' '+s.ble.next_try_s+' s':''));
 if(s.poll_enabled!==undefined){pollOn=s.poll_enabled;pollBtn()}
 if(s.debug_min!==undefined){dbgMin=s.debug_min;dbgBtn()}
 $('lerr').textContent=s.last_error?'Last error: '+s.last_error:'';$('safebox').classList.toggle('hide',!s.safe_mode);
 const lg=$('log'),atB=lg.scrollTop+lg.clientHeight>=lg.scrollHeight-5;lg.textContent=s.log.join('\n');if(atB)lg.scrollTop=lg.scrollHeight;
 if(s.ble.uuids||s.ble.name)$('uuids').textContent='Detected: '+[s.ble.name,s.ble.kind,s.ble.elm].filter(Boolean).join(' · ')+(s.ble.uuids?' (UUIDs '+s.ble.uuids.replace(/0x/g,'')+')':'');
 if($('devtime'))$('devtime').textContent='Time on the device'+': '+(s.time||'not synchronized yet');
 $('sysinfo').innerHTML=`Firmware ${esc(s.fw)}<br>${'Last restart'}: ${esc(s.reset_reason||'')}<br>${'Uptime'} ${age(s.uptime)}<br>${'Free heap'} ${Math.round(s.heap/1024)} KB (${'minimum'} ${Math.round((s.heap_min||0)/1024)} KB, ${'largest block'} ${Math.round((s.heap_block||0)/1024)} KB)<br>${'File system'} ${Math.round((s.fs_used||0)/1024)} / ${Math.round((s.fs_total||0)/1024)} KB<br>${'Poll cycles'} ${s.cycles}<br>IP ${esc(s.wifi.ip)} (${s.wifi.ip_mode==='static'?'static':s.wifi.ip_mode==='fallback'?'DHCP fallback – static IP did not work':'DHCP'})`;
 if($('ipfb'))$('ipfb').textContent=s.wifi.ip_mode==='fallback'?'The static IP did not work at the last start – currently via DHCP: '+s.wifi.ip:'';
}catch(e){}}
async function copyLog(){let txt='';try{txt=await (await fetch('/api/log')).text()}catch(e){txt=$('log').textContent}
 let ok=false;try{if(navigator.clipboard&&window.isSecureContext){await navigator.clipboard.writeText(txt);ok=true}}catch(e){}
 if(!ok){const ta=document.createElement('textarea');ta.value=txt;ta.style.position='fixed';ta.style.opacity='0';document.body.appendChild(ta);ta.select();try{ok=document.execCommand('copy')}catch(e){}ta.remove()}
 toast(ok?'Log copied to clipboard ('+txt.split('\n').length+' lines)':'Copy not possible – please use “Download”')}
async function copyText(txt){let ok=false;try{if(navigator.clipboard&&window.isSecureContext){await navigator.clipboard.writeText(txt);ok=true}}catch(e){}
 if(!ok){const ta=document.createElement('textarea');ta.value=txt;ta.style.position='fixed';ta.style.opacity='0';document.body.appendChild(ta);ta.select();try{ok=document.execCommand('copy')}catch(e){}ta.remove()}return ok}
async function runDiag(){if(!confirm('Create diagnostic report? The car will be woken up and every value queried once (approx. 1 min).'))return;
 const btn=$('diagbtn');btn.disabled=true;btn.textContent='Running…';
 const r=await runJob({type:'diag'},450,p=>{btn.textContent='Running…'+' '+p});btn.disabled=false;btn.textContent='Create diagnostic report';if(!r)return;
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
async function pollNow(){const r=await api('/api/pollnow',{method:'POST'});toast(r.msg)}
let pollOn=true;
async function togglePoll(){const on=!pollOn;if(!on&&!confirm('Switch off automatic polling? The bridge will no longer connect to the car until you switch it on again.'))return;
 const r=await api('/api/polling?on='+(on?1:0),{method:'POST'});toast(r.msg);pollOn=on;pollBtn();setTimeout(status,800)}
function pollBtn(){const b=$('pollsw');if(!b)return;b.textContent=pollOn?'Polling: ON':'Polling: OFF';b.classList.toggle('p',!pollOn)}
// ---------- Jobs ----------
async function runJob(b,max=120,onProg){let r=await post('/api/job',b);if(!r.ok){toast(r.msg);return null}
 for(let i=0;i<max;i++){await new Promise(x=>setTimeout(x,400));const j=await api('/api/job');if(onProg&&j.progress)onProg(j.progress);if(j.state==='done')return j.result||{}}return {ok:false,error:'Timeout'}}
function q(c){$('tcmd').value=c;termSend()}
async function termSend(){const c=$('tcmd').value.trim();if(!c)return;const o=$('tout');o.textContent+='> '+c+'\n';
 const r=await runJob({type:'raw',cmd:c});if(!r)return;o.textContent+=nl(r.ok?r.raw:'Error: '+(r.error||'')+(r.raw?'\n'+r.raw:''))+'\n\n';o.scrollTop=o.scrollHeight;$('tcmd').select()}
// ---------- Einstellungen ----------
const CF=['wifi_ssid','wifi_pass','hostname','web_pass','ip_mode','ip_addr','ip_mask','ip_gw','ip_dns1','ip_dns2','mqtt_host','mqtt_port','mqtt_user','mqtt_pass','mqtt_base','ha_prefix','device_name','ble_mac','ble_addr_type','ble_service','ble_notify','ble_write','ble_name','ble_pin','min_voltage','sleep_voltage','sleep_poll_min','low_batt_voltage','retry_sec','backoff_sec','cmd_timeout_ms','keep_connected','ntp_server','tz','log_persist','update_check'];
const NUM=['mqtt_port','min_voltage','sleep_voltage','sleep_poll_min','low_batt_voltage','retry_sec','backoff_sec','cmd_timeout_ms'];
async function loadCfg(){const c=await api('/api/config');CF.forEach(k=>{if($(k))$(k).value=String(c[k]??'')});
 const sel=$('tzsel'),has=[...sel.options].some(o=>o.value===c.tz);sel.value=has?c.tz:'custom';$('tz').classList.toggle('hide',has);ipModeChange();status()}
function ipModeChange(){$('ipbox').classList.toggle('hide',$('ip_mode').value!=='static')}
async function ipTakeCurrent(){const s=await api('/api/status');if(!s||!s.wifi)return;$('ip_addr').value=s.wifi.ip||'';$('ip_mask').value=s.wifi.mask||'';$('ip_gw').value=s.wifi.gw||'';$('ip_dns1').value=s.wifi.dns||'';toast('Applied – don’t forget to save')}
const isIp=v=>/^(25[0-5]|2[0-4]\d|1?\d?\d)(\.(25[0-5]|2[0-4]\d|1?\d?\d)){3}$/.test(v);
function tzChange(){const v=$('tzsel').value;if(v==='custom'){$('tz').classList.remove('hide');$('tz').focus()}else{$('tz').value=v;$('tz').classList.add('hide')}}
async function saveCfg(rb){if($('ip_mode').value==='static'){for(const k of ['ip_addr','ip_mask','ip_gw'])if(!isIp($(k).value.trim())){toast('Invalid IP: '+$(k).previousElementSibling.textContent);$(k).focus();return}
 for(const k of ['ip_dns1','ip_dns2'])if($(k).value.trim()&&!isIp($(k).value.trim())){toast('Invalid IP: '+$(k).previousElementSibling.textContent);$(k).focus();return}}
 const b={};CF.forEach(k=>{let v=$(k).value;if(NUM.includes(k))v=Number(v);if(k==='keep_connected'||k==='log_persist'||k==='update_check')v=v==='true';b[k]=v});
 const r=await post('/api/config'+(rb?'?reboot=1':''),b);toast(r.msg)}
async function scan(){const btn=$('scanbtn');btn.disabled=true;btn.textContent='Scanning (6 s)…';$('scanl').innerHTML='';
 const r=await runJob({type:'scan'});btn.disabled=false;btn.textContent='Scan for dongles';if(!r)return;
 $('scanl').innerHTML=(r.devices||[]).map(d=>`<div onclick="pick('${esc(d.mac)}',${d.random},this.dataset.n)" data-n="${esc(d.name||'')}"><span>${d.obd?'★ ':''}<b>${esc(d.name||'(no name)')}</b> <span class="mono">${esc(d.mac)}</span></span><span class="hint" style="margin:0">${d.rssi} dBm</span></div>`).join('')||'<p class="hint">'+'No devices found.'+'</p>'}
const bars=r=>r>=-55?'▂▄▆█':r>=-67?'▂▄▆':r>=-75?'▂▄':'▂';
async function wscan(){const btn=$('wscanbtn');btn.disabled=true;btn.textContent='Scanning…';$('wscanl').innerHTML='';let r=await api('/api/wifiscan?start=1');
 for(let i=0;i<40&&r.state!=='done';i++){await new Promise(x=>setTimeout(x,500));r=await api('/api/wifiscan')}
 btn.disabled=false;btn.textContent='Scan';const n=(r.networks||[]).sort((a,b)=>b.rssi-a.rssi);
 $('wscanl').innerHTML=n.map(w=>`<div data-s="${esc(w.ssid)}" onclick="wpick(this.dataset.s)"><span>${w.open?'':'🔒 '}<b>${esc(w.ssid)}</b> <span class="hint" style="margin:0">Channel ${w.ch}</span></span><span class="hint mono" style="margin:0">${bars(w.rssi)} ${w.rssi} dBm</span></div>`).join('')||'<p class="hint">'+'No networks found.'+'</p>'}
function wpick(s){$('wifi_ssid').value=s;$('wscanl').innerHTML='';$('wifi_pass').value='';$('wifi_pass').focus();toast('Enter password, then “Save & restart”')}
function pick(m,rnd,n){$('ble_mac').value=m;$('ble_name').value=n||'';$('ble_addr_type').value=rnd?'random':'public';toast('Applied – don’t forget to save')}
// ---------- Profile ----------
const DC=['','battery','voltage','current','power','energy','temperature','distance','speed','duration'];
let pnames={};const pname=id=>pnames[id]||id;
async function loadProfiles(){const r=await api('/api/profiles');active=r.active;pnames=r.names||{};
 const ids=[...r.list].sort((a,b)=>pname(a).localeCompare(pname(b)));
 $('psel').innerHTML=ids.map(n=>`<option value="${esc(n)}" ${n===(profName||active)?'selected':''}>${esc(pname(n))}${n===active?' ✓':''}</option>`).join('');
 await loadProfile(profName&&r.list.includes(profName)?profName:active)}
async function loadProfile(n){profName=n;prof=await api('/api/profile?name='+encodeURIComponent(n));
 $('psel').value=n;$('pact').textContent=n===active?'This profile is active.':'Active profile: '+pname(active);renderProfile()}
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
async function activate(){const r=await post('/api/config',{profile:profName});toast(r.ok?'Profile'+' '+pname(profName)+' activated':r.msg);active=profName;$('pact').textContent='This profile is active.';loadProfiles()}
async function delProfile(){if(!confirm('Profile'+' “'+pname(profName)+'” – delete?'))return;const r=await api('/api/profile-delete?name='+encodeURIComponent(profName),{method:'POST'});toast(r.msg);if(r.ok){profName='';loadProfiles()}}
function toggleRaw(){collect();$('rawbox').classList.toggle('hide');$('raw').value=JSON.stringify(prof,null,2)}
function rawApply(){try{prof=JSON.parse($('raw').value);renderProfile();toast('Applied – now save')}catch(e){toast('JSON error: '+e.message)}}
async function testPid(i){collect();const p=prof.pids[i];$('testcard').classList.remove('hide');$('testid').textContent=p.id;$('testout').innerHTML='<p class="hint">'+'Polling…'+'</p>';
 const r=await runJob({type:'test',header:p.header,cmd:p.cmd,formula:p.formula,cap:Number($('p_bat').value)||0,cons:Number($('p_cons').value)||0});if(!r){$('testout').innerHTML='';return}
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
const CHANGELOG=[{"v": "0.3.23", "en": ["New: online update (System → Online update) – checks the latest release on GitHub, shows the changes since the installed version and installs the *-esp32-ota.bin directly from the internet at the push of a button (encrypted connection with certificate check, size and image check, then restart)", "Automatic check 3 min after start and then daily (can be switched off in the settings); a new version is noted in the log", "Home Assistant: new update entity “Firmware” – shows installed and latest version, “Install” starts the online update", "Fix: “Download backup” produced an empty file (0 bytes) – the backup was built completely in RAM (JSON document, text and another copy for the response, together ~3 × 30 KB), which failed when memory was tight. It is now sent in pieces: settings, then every profile directly from its file", "Restore needs less memory: the uploaded file is parsed directly in the receive buffer instead of being copied twice"]}, {"v": "0.3.22", "en": ["WiFi: after a disconnect the bridge reconnects by itself every 30 s – the ESP32's automatic reconnect got stuck on some disconnect reasons until the watchdog restarted after 15 min", "WiFi: disconnect reason in the log (e.g. “reason 200: beacon timeout (weak signal / interference)”) with the last signal value; the signal strength is shown when connecting", "Time: implausible NTP times (before the firmware build or more than 5 years after it, e.g. 2034) are discarded and requested again; until a valid time is available the log shows the uptime"]}, {"v": "0.3.21", "en": ["Watchdog: after a hang the log shows where the firmware hung (e.g. “Firmware hung at: BLE subscribe/pairing”) – also in the diagnostic report", "BLE connection setup hardened: the watchdog is fed before every step, setup is aborted after 20 s and retried later (previously a hanging connection setup or PIN pairing could trigger the 60 s watchdog); the device name is only read on the first connection", "If the connection drops during initialisation, it is aborted immediately (instead of reporting every init line as “without prompt”)", "If the connection drops during polling, there is no 10 min pause (“No valid response”) any more – retry after 60 s; error “BLE disconnected” instead of “ATSH… failed”", "New: if the connection drops repeatedly right after connecting (reason 0x208), the bridge automatically uses slower BLE connection parameters (30–50 ms instead of 7.5–15 ms) – shown in the log, diagnostic report and status", "Warm-up: after connecting, a short “Tester Present” (3E00) is sent to the ECU before the first query – against the truncated first long response (only when the car is awake)", "Cleaner log: no [diag] init lines during the minute-by-minute voltage check while asleep; “BLE pairing OK” only the first time; “Dongle not reachable” while the car sleeps only once (dongle sleeps with the car), plus “Dongle reachable again”", "RSSI: invalid readings right after connecting (e.g. −128 dBm) are shown as “not measured yet”", "Diagnostic report: up to 3 connection attempts, clean abort with a note on connection loss, restart reason, BLE parameters, drops and RSSI at the end", "Fix: when scanning again the dongle name was missing (MAC only) – the name from the scan response is now remembered; for the configured dongle the known name is shown as a fallback", "Formulas: the last value of any other value is available by its ID (e.g. hv_voltage) – for quantities from two responses", "XPeng G9/G6/P7+: new values charging power (kW), charging and DC charging from HV current × HV voltage (first G9 test: 713 V × −10.5 A ≈ 7.5 kW while AC charging); “Charging status” is now called “raw value” (the G9 returned 1 while AC charging); existing XPeng profiles are extended automatically"]}, {"v": "0.3.20", "en": ["Fix: “Last error” in MQTT still stayed – the old value is retained in the broker and was never overwritten after a restart; also the reset waited for rarely polled values. Now: “none” after every error-free poll cycle, and the current state is sent immediately when MQTT connects"]}, {"v": "0.3.19", "en": ["Fix: “Last error” was never reset – as soon as all active values are read without errors again it is cleared (status page empty, Home Assistant “none”)"]}, {"v": "0.3.18", "en": ["Charging values set to 0 when the car falls asleep: if the car fell asleep right after charging, Home Assistant kept showing the last charging power – charging power, charging, DC charging, HV current (XPeng: charge status, HV current) are now set to 0 when the car falls asleep", "Profiles: new per-value field “sleep_zero” (in the JSON editor) – set the value to 0 when the car falls asleep; existing factory-derived profiles are extended automatically", "Note: the charging power is the power arriving in the battery (HV voltage × HV current) – measured at the wallbox it is about 10 % higher when AC charging (test: 3.7 kW in the battery at 4.1 kW at the wallbox)"]}, {"v": "0.3.17", "en": ["IONIQ 5: “Charging” and “DC charging” are now detected from the HV current (the charging bits in 220101 byte 12 always stay 0 on the IONIQ 5) – confirmed during AC charging: −7.0 A at 527.5 V = 3.7 kW", "IONIQ 5: new values HV current, HV voltage and charging power (kW); existing IONIQ profiles are extended automatically", "Formulas: new functions lt(a,b), gt(a,b), min(a,b), max(a,b)"]}, {"v": "0.3.16", "en": ["New: dongles with pairing PIN (e.g. WiCAN Pro via Bluetooth) – Settings → BLE dongle → “PIN / passkey”; the bridge pairs automatically (encrypted connection, stored bond), messages in the log; with a wrong PIN the bond is discarded", "Manual: section “Dongle with PIN (e.g. WiCAN Pro)”"]}, {"v": "0.3.15", "en": ["New: Profile → “Battery & range” – pick the battery variant from a list (known sizes pre-filled: IONIQ 5 58–84 kWh, G9, G6, P7+), capacity can be corrected by hand, consumption in kWh/100 km, calculated full range shown", "Formulas can use the variables CAP (usable kWh) and CONS (kWh/100 km); the range formulas of the factory profiles use them (“Test” uses the values from the form)", "Stored profiles are converted: old range formula “…/100*kWh/consumption” → CAP/CONS with the values taken over, battery selection list added", "IONIQ 5 factory profile: ATAT0 (no adaptive timing) and explicit flow control (ATFCSH7E4, ATFCSD300000, ATFCSM1) – long responses like 220101 almost never arrived complete with an “OBDII v1.5” clone, now 6 of 7 in a test; unmodified IONIQ profiles are converted automatically", "With user-defined flow control (ATFCSM1) the flow-control address follows every header change automatically (e.g. 7E4 → 7C6 for the odometer)", "MQTT: battery capacity and consumption in the profile attributes"]}, {"v": "0.3.14", "en": ["Fix: in sleep mode “BLE disconnected (reason 0x216)” was still logged every minute – filled the log in about 4 hours and pushed out older entries"]}, {"v": "0.3.13", "en": ["Lower memory use: log download and diagnostic report are sent in 1 KB chunks instead of being built completely in RAM – previously “Download/Copy” briefly needed up to ~40 KB of heap (measured minimum 25 KB)", "Diagnostic report is fetched via /api/diag (no longer inside the job JSON)"]}, {"v": "0.3.12", "en": ["New: diagnostic report (Status → Diagnostics) – fresh connection, every init response (rejected commands marked), ATI/AT@1/ATRV/ATDPN, every query of the profile with raw response, response time, indexed bytes and calculated values, summary; copy/download; without WiFi, IP and password data (dongle MAC shortened)", "New: diagnostic log for 2 hours – every request with raw response, init responses, voltage/protocol/MTU on each connection; switches itself off", "Init commands rejected by the dongle (“?”) are now always logged", "Manual: guide “Diagnostic report for beta testers”"]}, {"v": "0.3.11", "en": ["XPeng G6/G9/P7+: init sequence fixed – ATFCSD300000 added before ATFCSM1 (otherwise the ELM327 rejects the user-defined flow control and multi-frame responses never arrive; bug from the WiCAN profile), plus ATAL", "Stored profiles are repaired automatically at startup (missing ATFCSD added, 7DF queries disabled when a fixed receive filter ATCRA is set) – noted in the log", "XPeng factory profiles: “SoC (OBD2 standard)” removed – it could never answer because of the receive filter on 784", "Sleep mode with hysteresis like WiCAN: sleep below the threshold, awake only from threshold + 0.1 V", "Polling while asleep (every N min) only above 11.9 V", "New: 12V warning – Home Assistant sensor “12V low” (<base>/battery_low) below an adjustable threshold (default 12.0 V)"]}, {"v": "0.3.10", "en": ["BLE: larger packets (MTU 247) and shorter connection interval – more throughput against “Incomplete response” with long responses and weak signal; MTU is shown in the log", "Sleep mode: the check every minute no longer writes “BLE connected/disconnected” to the log each time (fewer flash writes)", "IONIQ 5 factory profile: “AC plug” removed – stays 0 even with the charging cable plugged in; existing profiles: disable or delete the value"]}, {"v": "0.3.9", "en": ["IONIQ 5 factory profile: “Charging BMS” (220106) removed – the bit is 1 whenever the car is awake (12 V top-up), even without a charging cable; existing profiles: disable or delete the value", "Sleep mode: if the 12 V voltage cannot be read, a sleeping car is left alone (previously it was polled and woken up)", "Log shows when “Poll now” wakes a sleeping car"]}, {"v": "0.3.8", "en": ["New: main switch “Polling ON/OFF” – on the status page and in Home Assistant as a switch (<base>/polling, command to <base>/polling/set). When OFF the bridge no longer connects to the car; Test, terminal and “Poll now” keep working. The setting survives a restart", "New: let the car sleep – if the 12 V voltage is below 13.2 V (car off, not charging), no requests are sent to the car, only the voltage at the dongle is measured. Previously polling every 2 min kept waking the IONIQ 5 (12 V jumped to 14.7 V each time)", "Settings → Polling: threshold “Car asleep below” and optional “Poll while car is asleep” (every N min); “Poll now” always polls", "Status shows “Car awake/asleep” with voltage; Home Assistant: new sensor “Car awake” (<base>/car_awake)", "Fix: the 10 min pause “No valid response” also triggered on incomplete responses although the car was awake – now only when the car does not respond at all", "IONIQ 5 factory profile: longer response timeout (ATSTFF) against incomplete multi-frame responses (existing profiles: change init command ATST96 → ATSTFF or restore factory profiles)", "Less CAN traffic: identical requests are sent only once per poll cycle (previously e.g. 220105 up to 3× per cycle)", "IONIQ 5 factory profile: new experimental values “Charging”, “DC charging”, “AC plug” (220101 byte 12, per WiCAN) and “Charging BMS” (220106 byte 27, per evDash) – please verify with Test"]}, {"v": "0.3.7", "en": ["Settings → WiFi: IP address automatic (DHCP) or static (IP, subnet mask, gateway, DNS); button “Use current values”", "Fallback: if the static IP does not work at startup (no WiFi connection or MQTT broker not reachable), the bridge automatically gets an address via DHCP – shown in the log, under System and in the settings", "Invalid entries (e.g. gateway not in subnet) are detected when saving or at startup"]}, {"v": "0.3.6", "en": ["Fix: crash loop caused by a full file system – the log files could use ~96 KB of 128 KB; now max. 2 × 16 KB, and the log never uses more than 70 % of the flash (profiles/settings take priority)", "Fix: log files are only read/written under a lock – downloading could collide with simultaneous writing/rotation", "Self-healing: after 2 crashes in a row the older log is deleted, after 3 the current one too; same at startup when the file system is too full; old 48 KB logs are removed on update", "System: free heap with minimum and largest block, plus file system usage", "Status in safe mode shows the MQTT connection correctly again", "Log lines right after startup use local time instead of UTC"]}, {"v": "0.3.5", "en": ["MQTT: active vehicle profile at <base>/profile (+ details as JSON at <base>/profile/attributes), in Home Assistant as sensor “Vehicle profile”", "MQTT: all values including profile and timestamp as one JSON at <base>/state after each poll cycle"]}, {"v": "0.3.4", "en": ["Time zone as an understandable selection list (e.g. “Central Europe – Germany, Austria, Switzerland …”) instead of a POSIX string; a custom value is still possible", "Current time on the device is shown below the time zone", "Dongle is shown with its name: Bluetooth name, detected type and ELM version (e.g. “vLinker MC · vLinker / iOS-Vlink · ELM327 v2.2”), also in the status and in the log"]}, {"v": "0.3.3", "en": ["Profile selection shows readable names instead of file names (e.g. “Hyundai IONIQ 5 / Kia EV6”, “XPeng G9”), sorted, active profile marked with ✓", "“Save copy” only asks for a name – the internal file name is generated automatically", "Factory profiles renamed: “Hyundai IONIQ 5 / Kia EV6”, “XPeng P7+ (experimental)”"]}, {"v": "0.3.2", "en": ["Factory profiles: all values enabled by default (IONIQ 5, G9, G6, P7+)", "Profile table: checkbox in the “Active” column header switches all values on/off"]}, {"v": "0.3.1", "en": ["New factory profiles: XPeng G6 and XPeng P7+ (P7+ experimental)", "Existing devices get the new profiles automatically after the update"]}, {"v": "0.3.0", "en": ["Complete interface, device messages and manual in German and English – language selector at the top right", "Language is stored on the device; log, error messages and Home Assistant names follow the selection", "Factory profiles with German or English value names", "Bilingual changelog (CHANGELOG.md / CHANGELOG.en.md)"]}, {"v": "0.2.5", "en": ["Back up settings and profiles (JSON file, optionally including passwords)", "Restore a backup – settings and/or profiles, followed by an automatic restart"]}, {"v": "0.2.4", "en": ["User manual built into the device (link “Help” at the top right or /hilfe)", "Manual with search box and live check of your own system with links to the matching help"]}, {"v": "0.2.3", "en": ["Log with real time of day (NTP, configurable time zone; before sync the uptime “+hh:mm:ss”)", "Store log permanently in flash (survives restarts, rotates at 48 KB), can be disabled", "Log: copy, download, download older log, clear", "Changelog in the System tab"]}, {"v": "0.2.2", "en": ["Warm-up after dongle reset (ATRV + pause) – fixes CAN ERROR on the Test button", "Single retry on CAN ERROR", "BLE stays connected for 60 s after Test/Terminal"]}, {"v": "0.2.1", "en": ["Late ELM prompts (“>”) are ignored – fixes truncated responses", "Wait until the dongle is quiet before each command", "Up to 2 retries on incomplete responses, raw response in the log"]}, {"v": "0.2.0", "en": ["Watchdog: hang > 60 s, WiFi 15 min or MQTT 20 min down → restart", "Safe mode after 3 crashes in a row (no BLE, web UI reachable)", "Reason of the last restart in the System tab"]}, {"v": "0.1.10", "en": ["Fix: “Restore factory profiles” and “Delete profile” reported “Invalid profile name”"]}, {"v": "0.1.9", "en": ["Incomplete multi-frame responses are detected and retried", "IONIQ 5: init command ATST96 (longer timeout)", "Clearer log message for scheduled BLE disconnects"]}, {"v": "0.1.8", "en": ["Status LED removed (ESP32-DevKitC V4 has no controllable LED)"]}, {"v": "0.1.7", "en": ["Firmware update with real progress, error messages and waiting for the restart", "Check for factory.bin, BLE polling pauses during the update"]}, {"v": "0.1.5 – 0.1.6", "en": ["Version number in the file name and in the web interface", "LED test (removed later)"]}, {"v": "0.1.4", "en": ["XPeng G9: SoH 22110A, odometer 3 bytes, CLTC range 221118, charge limit/charge status"]}, {"v": "0.1.3", "en": ["Battery temperature, odometer, calculated range; G9 profile based on WiCAN"]}, {"v": "0.1.1", "en": ["WiFi scan in the settings"]}, {"v": "0.1.0", "en": ["First version: BLE ELM327 → MQTT with Home Assistant discovery, web interface, profiles, test, terminal, OTA"]}];
function renderChangelog(){$('changelog').innerHTML=CHANGELOG.map((c,i)=>`<div style="margin-bottom:10px"><b>v${esc(c.v)}</b>${i==0?' <span class="pill" style="padding:0 7px;font-size:11px">new</span>':''}<ul style="margin:4px 0 0 18px;padding:0">${c.en.map(x=>`<li>${esc(x)}</li>`).join('')}</ul></div>`).join('')}
renderChangelog();
status();setInterval(()=>{if(cur==='status'||cur==='sys'||cur==='cfg')status()},3000);
</script></body></html>)HTML";
