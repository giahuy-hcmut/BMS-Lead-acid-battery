#ifndef WEB_HTML_H
#define WEB_HTML_H

#include <Arduino.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>MURATA EV BMS</title>
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
  <style>
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

    :root {
      --bg:      #0d1117;
      --surface: #161b22;
      --border:  #21262d;
      --green:   #00E676;
      --cyan:    #39d0d8;
      --red:     #ff4d4d;
      --yellow:  #ffc107;
      --text:    #e6edf3;
      --muted:   #7d8590;
    }

    body {
      font-family: 'Segoe UI', system-ui, sans-serif;
      background: var(--bg);
      color: var(--text);
      min-height: 100vh;
      max-width: 480px;
      margin: 0 auto;
    }

    /* HEADER */
    .header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 12px 16px;
      background: var(--surface);
      border-bottom: 1px solid var(--border);
    }
    .header-title { color: var(--green); font-size: 14px; font-weight: 600; letter-spacing: 1px; }
    .header-time  { color: var(--muted); font-size: 13px; font-variant-numeric: tabular-nums; }

    /* STATUS BAR */
    .status-bar {
      display: flex;
      justify-content: space-around;
      padding: 10px 16px;
      background: var(--surface);
      border-bottom: 1px solid var(--border);
    }
    .status-item { font-size: 13px; color: var(--muted); }
    .status-on  { font-weight: 700; color: var(--green); }
    .status-off { font-weight: 700; color: var(--red); }

    /* HERO */
    .hero {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 1px;
      background: var(--border);
      border-bottom: 1px solid var(--border);
    }
    .hero-cell  { background: var(--bg); padding: 20px 16px; text-align: center; }
    .hero-val   { font-size: 42px; font-weight: 700; color: var(--green); font-variant-numeric: tabular-nums; line-height: 1; }
    .hero-unit  { font-size: 18px; color: var(--muted); vertical-align: super; }
    .hero-label { font-size: 11px; color: var(--muted); letter-spacing: 1px; text-transform: uppercase; margin-top: 6px; }

    /* STATS GRID */
    .stats-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 1px;
      background: var(--border);
      border-bottom: 1px solid var(--border);
    }
    .stat-row {
      background: var(--bg);
      padding: 10px 16px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      font-size: 13px;
    }
    .stat-label { color: var(--muted); }
    .stat-val         { color: var(--cyan);  font-weight: 600; font-variant-numeric: tabular-nums; }
    .stat-val.green   { color: var(--green); }

    /* SECTION TITLE */
    .section-title {
      font-size: 11px;
      letter-spacing: 2px;
      text-transform: uppercase;
      color: var(--muted);
      padding: 14px 16px 8px;
      border-bottom: 1px solid var(--border);
    }

    /* PACK ROWS */
    .pack-row {
      display: flex;
      align-items: center;
      gap: 12px;
      padding: 12px 16px;
      border-bottom: 1px solid var(--border);
    }
    .pack-num {
      width: 34px;
      height: 34px;
      border-radius: 50%;
      background: #1c2a3a;
      border: 1.5px solid var(--cyan);
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: 12px;
      font-weight: 700;
      color: var(--cyan);
      flex-shrink: 0;
    }
    .pack-num.offline { border-color: var(--red);    color: var(--red);    background: #2a1c1c; }
    .pack-num.error   { border-color: var(--yellow); color: var(--yellow); background: #2a2310; }

    .pack-volt { font-size: 22px; font-weight: 700; color: var(--text); font-variant-numeric: tabular-nums; min-width: 82px; }
    .pack-volt sup { font-size: 12px; color: var(--muted); }

    .pack-meta { display: flex; flex-direction: column; gap: 3px; flex: 1; }
    .pack-meta-row { font-size: 12px; color: var(--muted); }
    .pack-meta-val { color: var(--text); }

    .pack-status { font-size: 11px; font-weight: 700; padding: 3px 8px; border-radius: 4px; letter-spacing: 0.5px; white-space: nowrap; }
    .pack-status.online  { background: rgba(0,230,118,0.1); color: var(--green); }
    .pack-status.offline { background: rgba(255,77,77,0.1);  color: var(--red); }
    .pack-status.error   { background: rgba(255,193,7,0.1);   color: var(--yellow); }

    /* CONTROL */
    .control-row {
      display: flex;
      align-items: center;
      gap: 14px;
      padding: 14px 16px;
      border-bottom: 1px solid var(--border);
    }
    .control-icon {
      width: 36px; height: 36px;
      border-radius: 8px;
      background: var(--surface);
      display: flex; align-items: center; justify-content: center;
      font-size: 18px;
      flex-shrink: 0;
    }
    .control-label { font-size: 14px; }
    .control-sub   { font-size: 11px; color: var(--muted); margin-top: 2px; }

    .toggle { position: relative; width: 46px; height: 26px; flex-shrink: 0; }
    .toggle input { display: none; }
    .toggle-slider {
      position: absolute; inset: 0;
      background: #333;
      border-radius: 13px;
      cursor: pointer;
      transition: background 0.25s;
    }
    .toggle-slider::before {
      content: '';
      position: absolute;
      width: 20px; height: 20px;
      left: 3px; top: 3px;
      background: #fff;
      border-radius: 50%;
      transition: transform 0.25s;
    }
    .toggle input:checked + .toggle-slider              { background: var(--green); }
    .toggle input:checked + .toggle-slider::before      { transform: translateX(20px); }

    .wake-btn {
      padding: 8px 14px;
      background: rgba(57,208,216,0.1);
      border: 1px solid var(--cyan);
      color: var(--cyan);
      border-radius: 6px;
      font-size: 13px; font-weight: 600;
      cursor: pointer;
      letter-spacing: 0.5px;
      flex-shrink: 0;
    }
    .wake-btn:active { background: rgba(57,208,216,0.25); }
  </style>
</head>
<body>

  <div class="header">
    <div class="header-title">MURATA EV BMS</div>
    <div class="header-time" id="clock">--:--:--</div>
  </div>

  <div class="status-bar">
    <div class="status-item">Power: <span id="st-power" class="status-on">--</span></div>
    <div class="status-item">System: <span id="st-system" class="status-on">--</span></div>
    <div class="status-item">Battery: <span id="st-battery" class="status-on">--</span></div>
  </div>

  <div id="fault-banner" style="display:none; margin:12px 16px; padding:12px 16px; background:rgba(255,77,77,0.08); border:1px solid #ff4d4d; border-radius:8px;">
    <div style="color:#ff4d4d; font-weight:700; font-size:14px;">&#9888; PROTECTION ACTIVE</div>
    <div id="fault-reason" style="color:#7d8590; font-size:12px; margin-top:4px;"></div>
  </div>

  <div class="hero">
    <div class="hero-cell">
      <div class="hero-val"><span id="totalVolt">--.-</span><span class="hero-unit">V</span></div>
      <div class="hero-label">Total Voltage</div>
    </div>
    <div class="hero-cell">
      <div class="hero-val"><span id="sysCurr">--.-</span><span class="hero-unit">A</span></div>
      <div class="hero-label">Current</div>
    </div>
  </div>

  <div class="stats-grid">
    <div class="stat-row">
      <span class="stat-label">Power</span>
      <span class="stat-val" id="sysPow">-- W</span>
    </div>
    <div class="stat-row">
      <span class="stat-label">SOC</span>
      <span class="stat-val green" id="sysSOC">-- %</span>
    </div>
    <div class="stat-row">
      <span class="stat-label">Remain</span>
      <span class="stat-val" id="remainAh">-- Ah</span>
    </div>
    <div class="stat-row">
      <span class="stat-label">Volt Diff</span>
      <span class="stat-val" id="voltDiff">-- V</span>
    </div>
  </div>

  <div class="section-title">Battery Packs</div>
  <div id="packs-container"></div>

  <div class="section-title">Control</div>

  <div class="control-row">
    <div class="control-icon">&#9889;</div>
    <div style="flex:1">
      <div class="control-label">Main Relay</div>
      <div class="control-sub">OFF cuts power to load</div>
    </div>
    <label class="toggle">
      <input type="checkbox" id="relay-toggle" checked onchange="setRelay(this.checked)">
      <span class="toggle-slider"></span>
    </label>
  </div>

  <div class="control-row">
    <div class="control-icon">&#128276;</div>
    <div style="flex:1">
      <div class="control-label">Slaves Active</div>
      <div class="control-sub">OFF &#8594; slaves sleep after 5s</div>
    </div>
    <label class="toggle">
      <input type="checkbox" id="slaves-toggle" checked onchange="setSlaves(this.checked)">
      <span class="toggle-slider"></span>
    </label>
  </div>

<script>
  var CAPACITY_AH = 20.0;

  function pad(n) { return String(n).padStart(2, '0'); }

  function tick() {
    var d = new Date();
    document.getElementById('clock').textContent =
      pad(d.getHours()) + ':' + pad(d.getMinutes()) + ':' + pad(d.getSeconds());
  }
  setInterval(tick, 1000);
  tick();

  function renderData(data) {
    document.getElementById('totalVolt').textContent = data.totalV.toFixed(2);
    document.getElementById('sysCurr').textContent   = data.sysI.toFixed(2);
    // soc < 0 nghia la System_MinSoc() tra -1: co bình offline nen KHONG BIET
    // binh mat tich co phai binh yeu nhat hay khong. Hien "--" thay vi doan.
    var socKnown = (data.soc >= 0);
    document.getElementById('sysSOC').textContent    = socKnown ? (data.soc + ' %') : '-- %';
    document.getElementById('sysPow').textContent    = (data.totalV * data.sysI).toFixed(1) + ' W';
    document.getElementById('remainAh').textContent  = socKnown
        ? ((data.soc / 100 * CAPACITY_AH).toFixed(1) + ' Ah')
        : '-- Ah';

    var onlineVolts = data.packs.filter(function(p) { return p.online; }).map(function(p) { return p.volt; });
    if (onlineVolts.length >= 2) {
      var diff = Math.max.apply(null, onlineVolts) - Math.min.apply(null, onlineVolts);
      document.getElementById('voltDiff').textContent = diff.toFixed(3) + ' V';
    } else {
      document.getElementById('voltDiff').textContent = '-- V';
    }

    var html = '';
    for (var i = 0; i < data.packs.length; i++) {
      var p       = data.packs[i];
      var hasErr  = p.online && p.err > 0;
      var numCls  = !p.online ? 'offline' : (hasErr ? 'error' : '');
      var stCls   = !p.online ? 'offline' : (hasErr ? 'error'  : 'online');
      var stText  = !p.online ? 'LOST'    : (hasErr ? 'ERROR'  : 'ONLINE');
      var voltStr = p.online ? p.volt.toFixed(2) : '--.-';
      // SOC cua rieng binh nay, tu bo Kalman tren slave do. Binh offline thi so
      // luu lai la so CU -> hien '--', cung cach doi xu nhu volt va temp.
      var pSocStr = p.online ? (p.soc + ' %') : '--';
      var tempStr = p.online ? p.temp + '&#176;C' : '--';
      var errStr  = p.online ? ('0x' + p.err.toString(16).padStart(2,'0').toUpperCase()) : '--';

      html += '<div class="pack-row">';
      html += '<div class="pack-num ' + numCls + '">' + pad(i + 1) + '</div>';
      html += '<div class="pack-volt">' + voltStr + '<sup>V</sup></div>';
      html += '<div class="pack-meta">';
      html += '<div class="pack-meta-row">SOC: <span class="pack-meta-val">' + pSocStr + '</span></div>';
      html += '<div class="pack-meta-row">Temp: <span class="pack-meta-val">' + tempStr + '</span></div>';
      html += '<div class="pack-meta-row">Error: <span class="pack-meta-val">' + errStr + '</span></div>';
      html += '</div>';
      html += '<div class="pack-status ' + stCls + '">' + stText + '</div>';
      html += '</div>';
    }
    document.getElementById('packs-container').innerHTML = html;
  }

  function setRelay(isOn) {
    var body = new URLSearchParams();
    body.append('state', isOn ? 'on' : 'off');
    fetch('/relay', { method: 'POST', body: body });
  }

  function setSlaves(isOn) {
    var body = new URLSearchParams();
    body.append('state', isOn ? 'on' : 'off');
    fetch('/slaves', { method: 'POST', body: body });
  }

  function updateStatusBar(data) {
    var powerOn = !data.relayOff && !data.locked;
    var allOk   = !data.locked;
    for (var i = 0; i < data.packs.length; i++) {
      if (!data.packs[i].online || data.packs[i].err > 0) { allOk = false; break; }
    }

    var stPower   = document.getElementById('st-power');
    var stSystem  = document.getElementById('st-system');
    var stBattery = document.getElementById('st-battery');

    stPower.textContent  = powerOn ? 'ON' : 'OFF';
    stPower.className    = powerOn ? 'status-on' : 'status-off';

    stSystem.textContent = allOk ? 'OK' : 'WARNING';
    stSystem.className   = allOk ? 'status-on' : 'status-off';

    stBattery.textContent = data.slavesActive ? 'Awake' : 'Sleep';
    stBattery.className   = data.slavesActive ? 'status-on' : 'status-off';

    var banner = document.getElementById('fault-banner');
    if (data.locked && data.fault) {
      banner.style.display = 'block';
      document.getElementById('fault-reason').textContent = data.fault;
    } else {
      banner.style.display = 'none';
    }
  }

  if (window.EventSource) {
    var source = new EventSource('/events');
    source.addEventListener('message', function(e) {
      var data = JSON.parse(e.data);
      var relayToggle  = document.getElementById('relay-toggle');
      var slavesToggle = document.getElementById('slaves-toggle');
      if (relayToggle)  relayToggle.checked  = !data.relayOff && !data.locked;
      if (slavesToggle) slavesToggle.checked = data.slavesActive;
      updateStatusBar(data);
      renderData(data);
    });
  }
</script>
</body>
</html>
)rawliteral";

#endif
