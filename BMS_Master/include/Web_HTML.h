#ifndef WEB_HTML_H
#define WEB_HTML_H

#include <Arduino.h>

// Lưu toàn bộ giao diện Web vào bộ nhớ Flash (PROGMEM) để không tốn RAM
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>MURATA EV Dashboard</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #121212; color: #ffffff; text-align: center; margin: 0; padding: 20px; }
    h1 { color: #00E676; letter-spacing: 2px; text-transform: uppercase; }
    .card-container { display: flex; flex-wrap: wrap; justify-content: center; gap: 15px; margin-top: 20px; }
    .card { background-color: #1e1e1e; border-radius: 10px; padding: 20px; width: 140px; box-shadow: 0 4px 8px rgba(0,255,118,0.1); border-top: 3px solid #333; }
    .card.online { border-top-color: #00E676; }
    .card.offline { border-top-color: #ff3d00; opacity: 0.6; }
    .value { font-size: 24px; font-weight: bold; margin: 10px 0; }
    .label { font-size: 12px; color: #aaaaaa; }
    .main-stats { display: flex; justify-content: center; gap: 20px; margin-bottom: 30px; }
    .main-card { background-color: #1a1a2e; width: 200px; padding: 25px; border-radius: 15px; border: 1px solid #0f3460; }
    .main-val { font-size: 36px; font-weight: bold; color: #00E676; }
  </style>
</head>
<body>
  <h1>MURATA EV BMS</h1>
  
  <div class="main-stats">
    <div class="main-card">
      <div class="label">TOTAL VOLTAGE</div>
      <div class="main-val"><span id="totalVolt">--.--</span> V</div>
    </div>
    <div class="main-card">
      <div class="label">SYSTEM CURRENT</div>
      <div class="main-val"><span id="sysCurr">--.--</span> A</div>
    </div>
  </div>

  <div class="label">BATTERY PACKS STATUS</div>
  <div class="card-container" id="packs-container">
    </div>

<script>
  // Kỹ thuật Server-Sent Events (SSE) bắt dữ liệu không cần load lại trang
  if (!!window.EventSource) {
    var source = new EventSource('/events');
    
    source.addEventListener('message', function(e) {
      var data = JSON.parse(e.data);
      
      // Cập nhật thông số tổng
      document.getElementById("totalVolt").innerHTML = data.totalV.toFixed(2);
      document.getElementById("sysCurr").innerHTML = data.sysI.toFixed(2);
      
      // Vẽ lại các Pack pin
      var packsHTML = "";
      for(var i=0; i < data.packs.length; i++) {
        var p = data.packs[i];
        var statusClass = p.online ? "online" : "offline";
        var statusText = p.online ? "ONLINE" : "LOST";
        var voltText = p.online ? p.volt.toFixed(2) + " V" : "--.-- V";
        
        packsHTML += '<div class="card ' + statusClass + '">';
        packsHTML += '<div class="label">PACK ' + (i+1) + '</div>';
        packsHTML += '<div class="value">' + voltText + '</div>';
        packsHTML += '<div class="label">' + statusText + '</div>';
        packsHTML += '</div>';
      }
      document.getElementById("packs-container").innerHTML = packsHTML;
    }, false);
  }
</script>
</body>
</html>
)rawliteral";

#endif