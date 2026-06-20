#include <Arduino.h>

const char portalHTML[] PROGMEM = R"====(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>WiFi Portal</title>
<style>
body {
  font-family: sans-serif;
  text-align: center;
  background: #f1f1f1;
  margin: 0;
}
.card {
  background: #fff;
  width: 90%;
  max-width: 360px;
  padding: 20px;
  margin: 60px auto;
  border-radius: 18px;
  box-shadow: 0 5px 20px rgba(0,0,0,0.15);
}
.loader {
  width: 38px; height: 38px;
  border: 4px solid #ddd;
  border-top-color: #3b82f6;
  border-radius: 50%;
  margin: 20px auto;
  animation: spin 1s linear infinite;
}
@keyframes spin { 100% { transform: rotate(360deg); } }
.btn {
  padding: 12px 20px;
  background: #3b82f6;
  color: white;
  border-radius: 10px;
  display: inline-block;
  margin-top: 15px;
  text-decoration: none;
}
#status { font-size: 14px; opacity: 0.7; margin-top: 14px; }
</style>
</head>

<body>
<div class="card">
  <h2>🔒 Secure WiFi Portal</h2>
  <p>Your connection is being authenticated...</p>
  <div class="loader"></div>

  <a class="btn" onclick="sendAction('connect')">Retry Connection</a>
  <div id="status">Checking device...</div>
</div>

<script>
function sendAction(type){
    fetch('/event?a=' + type)
    .then(r => r.text())
    .then(t => {
        document.getElementById("status").innerHTML = "Device: " + t;
    });
}

setInterval(() => {
    fetch('/status')
    .then(r => r.json())
    .then(j => {
        document.getElementById("status").innerHTML =
        "Device Status: " + j.device + " | AP: " + j.ap;
    });
}, 1000);
</script>
</body>
</html>
)====";
