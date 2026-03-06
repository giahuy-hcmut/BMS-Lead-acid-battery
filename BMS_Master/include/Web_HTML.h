#ifndef WEB_HTML_H
#define WEB_HTML_H

#include <Arduino.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>MURATA EV Dashboard</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #121212; color: #ffffff; text-align: center; margin: 0; padding: 20px; }
    h1 { color: #00E676; letter-spacing: 2px; text-transform: uppercase; font-size: 26px;}
    .top-stats { display: flex; justify-content: center; gap: 20px; margin-bottom: 30px; flex-wrap: wrap;}
    .stat-box { background-color: #1e1e1e; padding: 20px; border-radius: 12px; min-width: 160px; box-shadow: 0 4px 10px rgba(0,230,118,0.1); border-top: 2px solid #333;}
    .label { font-size: 13px; color: #888; margin-bottom: 5px; letter-spacing: 1px;}
    .main-val { font-size: 32px; font-weight: bold; color: #00E676; }
    
    .card-container { display: flex; flex-wrap: wrap; justify-content: center; gap: 15px; margin-top: 20px; }
    .card { background-color: #1e1e1e; border-radius: 10px; padding: 15px; width: 150px; box-shadow: 0 4px 8px rgba(0,0,0,0.3); border-top: 4px solid #333; }
    .card.online { border-top-color: #00E676; }
    .card.offline { border-top-color: #ff3d00; opacity: 0.6; }
    
    .card .val { font-size: 24px; font-weight: bold; margin: 10px 0; color: #fff;}
    .card .sub-val { font-size: 14px; color: #bbb; margin: 5px 0; display: flex; justify-content: space-between;}
    .card .status { font-size: 12px; font-weight: bold; margin-top: 15px; padding: 6px; border-radius: 6px;}
    
    .card.online .status { background-color: rgba(0, 230, 118, 0.1); color: #00E676; }
    .card.offline .status { background-color: rgba(255, 61, 0, 0.1); color: #ff3d00; }
    
    .text-red { color: #ff3d00 !important; font-weight: bold;}
  </style>
</head>
<body>
  <h1>MURATA EV Dashboard</h1>
  
  <div class="top-stats">
    <div class="stat-box">
      <div class="label">SYSTEM VOLTAGE</div>
      <div class="main-val"><span id="totalVolt">--.--</span> V</div>
    </div>
    <div class="stat-box">
      <div class="label">SYSTEM CURRENT</div>
      <div class="main-val"><span id="sysCurr">--.--</span> A</div>
    </div>
    <div class="stat-box">
      <div class="label">SYSTEM SOC</div>
      <div class="main-val"><span id="sysSOC">--</span> %</div>
    </div>
  </div>

  <div class="label" style="font-size: 16px; margin-top: 30px; color: #ccc;">BATTERY PACKS DETAILS</div>
  <div class="card-container" id="packs-container">
    </div>

<script>
  if (!!window.EventSource) {
    var source = new EventSource('/events');
    
    source.addEventListener('message', function(e) {
      var data = JSON.parse(e.data);
      
      // 1. Cập nhật thống kê tổng quan
      document.getElementById("totalVolt").innerHTML = data.totalV.toFixed(2);
      document.getElementById("sysCurr").innerHTML = data.sysI.toFixed(2);
      document.getElementById("sysSOC").innerHTML = data.soc;
      
      // 2. Cập nhật chi tiết từng bình ắc quy
      var packsHTML = "";
      for(var i=0; i < data.packs.length; i++) {
        var p = data.packs[i];
        var statusClass = p.online ? "online" : "offline";
        var statusText = p.online ? "ONLINE" : "LOST";
        var voltText = p.online ? p.volt.toFixed(2) + " V" : "--.-- V";
        var tempText = p.online ? p.temp + " &deg;C" : "--";
        
        // Hiển thị mã lỗi Hexadecimal (VD: 0x01)
        var errStr = "0x00";
        var errClass = "";
        if(p.online) {
            errStr = "0x" + p.err.toString(16).padStart(2, '0').toUpperCase();
            if(p.err > 0) errClass = "text-red"; // Báo đỏ nếu có lỗi
        } else {
            errStr = "--";
        }
        
        packsHTML += '<div class="card ' + statusClass + '">';
        packsHTML += '<div class="label">PACK ' + (i+1) + '</div>';
        packsHTML += '<div class="val">' + voltText + '</div>';
        packsHTML += '<div class="sub-val"><span>Temp:</span> <span>' + tempText + '</span></div>';
        packsHTML += '<div class="sub-val ' + errClass + '"><span>Error:</span> <span>' + errStr + '</span></div>';
        packsHTML += '<div class="status">' + statusText + '</div>';
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