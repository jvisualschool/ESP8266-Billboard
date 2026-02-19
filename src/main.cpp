#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

// OLED 설정
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 14
#define OLED_SCL 12

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
ESP8266WebServer server(80);

// 전역 변수
String currentMsg = "Mini Billboard Online";
int currentAnim = 1; // 0: Static, 1: Scroll, 2: Blink, 3: Typewriter, 4: Pulse. Default to scroll to show life.
unsigned long lastUpdate = 0;
int xPos = SCREEN_WIDTH;
bool blinkState = true;
int charIndex = 0;
float scale = 1.0;
bool scaleUp = true;
unsigned long statusMillis = 0;
bool statusDot = false;

// --- HTML 소스 (Premium Design) ---
const char* HTML_CONTENT = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <title>ESP8266 Mini Billboard</title>
    <style>
        @import url('https://fonts.googleapis.com/css2?family=Orbitron:wght@400;700&family=Noto+Sans+KR:wght@300;500&display=swap');
        :root { --primary: #00d4ff; --bg: #0a0b1e; }
        body { 
            margin: 0; padding: 0; background: var(--bg); color: white; 
            font-family: 'Noto Sans KR', sans-serif; height: 100vh;
            display: flex; align-items: center; justify-content: center;
            background-image: radial-gradient(circle at 50% 50%, #151a3a 0%, #0a0b1e 100%);
        }
        .glass-card {
            background: rgba(255, 255, 255, 0.05);
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255, 255, 255, 0.1);
            border-radius: 24px;
            padding: 40px;
            width: 90%; max-width: 400px;
            box-shadow: 0 25px 50px rgba(0,0,0,0.5);
            text-align: center;
        }
        h1 { 
            font-family: 'Orbitron', sans-serif; font-size: 1.5rem; 
            color: var(--primary); text-shadow: 0 0 15px var(--primary);
            margin-bottom: 30px; letter-spacing: 2px;
        }
        .input-group { margin-bottom: 25px; text-align: left; }
        label { color: rgba(255,255,255,0.6); font-size: 0.8rem; margin-bottom: 8px; display: block; }
        input[type="text"] {
            width: 100%; padding: 12px 16px; border-radius: 12px;
            background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.1);
            color: white; font-size: 1rem; box-sizing: border-box; outline: none; transition: 0.3s;
        }
        input[type="text"]:focus { border-color: var(--primary); box-shadow: 0 0 10px rgba(0,212,255,0.3); }
        select {
            width: 100%; padding: 12px 16px; border-radius: 12px;
            background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.1);
            color: white; font-size: 1rem; box-sizing: border-box; outline: none; cursor: pointer;
        }
        .btn {
            width: 100%; padding: 14px; border-radius: 12px; background: var(--primary);
            color: #000; font-weight: bold; border: none; cursor: pointer; font-family: 'Orbitron';
            transition: 0.3s; box-shadow: 0 0 20px rgba(0,212,255,0.4); margin-top: 10px;
        }
        .btn:hover { transform: translateY(-2px); box-shadow: 0 0 30px rgba(0,212,255,0.6); }
        .status { margin-top: 20px; font-size: 0.8rem; color: var(--primary); opacity: 0.8; }
    </style>
</head>
<body>
    <div class="glass-card">
        <h1>BILLBOARD CONTROL</h1>
        <div class="input-group">
            <label>DISPLAY TEXT</label>
            <input type="text" id="msg" placeholder="Enter message here..." maxlength="32">
        </div>
        <div class="input-group">
            <label>ANIMATION STYLE</label>
            <select id="anim">
                <option value="0">STATIC CENTER</option>
                <option value="1">SMOOTH SCROLL</option>
                <option value="2">NEON BLINK</option>
                <option value="3">CYBER TYPEWRITER</option>
                <option value="4">PULSE SYNC</option>
            </select>
        </div>
        <button class="btn" onclick="update()">SEND TO DISPLAY</button>
        <div id="status" class="status">System Ready</div>
    </div>
    <script>
        function update() {
            const msg = document.getElementById('msg').value;
            const anim = document.getElementById('anim').value;
            const status = document.getElementById('status');
            status.innerText = "Transmitting...";
            fetch(`/update?msg=${encodeURIComponent(msg)}&anim=${anim}`)
                .then(r => r.text())
                .then(data => {
                    status.innerText = "Display Updated Successfully!";
                    setTimeout(() => status.innerText = "System Ready", 2000);
                });
        }
    </script>
</body>
</html>
)rawliteral";

// --- 애니메이션 함수들 ---

void drawStatic(String msg) {
  display.setTextSize(2);
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, (52 - h) / 2);
  display.print(msg);
}

void drawScroll(String msg) {
  display.setTextSize(2); // Reduced from 3 to 2 for better fit
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
  
  if (millis() - lastUpdate > 30) {
    xPos -= 2;
    if (xPos < - (int)w) xPos = SCREEN_WIDTH;
    lastUpdate = millis();
  }
  // Vertical center within the top 52px (above the footer)
  display.setCursor(xPos, (52 - h) / 2);
  display.print(msg);
}

void drawBlink(String msg) {
  display.setTextSize(2);
  if (millis() - lastUpdate > 500) {
    blinkState = !blinkState;
    lastUpdate = millis();
  }
  if (blinkState) {
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, (52 - h) / 2);
    display.print(msg);
  }
}

void drawTypewriter(String msg) {
  display.setTextSize(2);
  if (millis() - lastUpdate > 150) {
    charIndex++;
    if (charIndex > (int)msg.length()) {
       if (millis() - lastUpdate > 2000) charIndex = 0; // Wait at end
    } else {
       lastUpdate = millis();
    }
  }
  String sub = msg.substring(0, charIndex);
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(sub, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, (52 - h) / 2);
  display.print(sub);
  if (millis() % 1000 < 500) display.print("_");
}

void drawPulse(String msg) {
  if (millis() - lastUpdate > 40) {
    if (scaleUp) scale += 0.05; else scale -= 0.05;
    if (scale > 1.3) scaleUp = false;
    if (scale < 0.7) scaleUp = true;
    lastUpdate = millis();
  }
  display.setTextSize(2);
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
  
  // Simple version: use size 3 when scale is high
  display.setTextSize((scale > 1.1) ? 3 : 2);
  display.getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, (52 - h) / 2);
  display.print(msg);
}

// --- 서버 핸들러 ---

void handleRoot() {
  server.send(200, "text/html", HTML_CONTENT);
}

void handleUpdate() {
  if (server.hasArg("msg")) {
    currentMsg = server.arg("msg");
    if (currentMsg == "") currentMsg = "Empty";
  }
  if (server.hasArg("anim")) {
    currentAnim = server.arg("anim").toInt();
  }
  xPos = SCREEN_WIDTH;
  charIndex = 0;
  scale = 1.0;
  server.send(200, "text/plain", "OK");
  Serial.print("New Message: "); Serial.println(currentMsg);
}

void setup() {
  Serial.begin(74880);
  Wire.begin(OLED_SDA, OLED_SCL);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // WiFi 연결 시도 (무한 루프)
  bool connected = false;
  int retryCount = 0;
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  while (!connected) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("NETWORK CONNECTING");
    display.drawFastHLine(0, 12, 128, SSD1306_WHITE);
    
    display.setCursor(0, 20);
    display.print("SSID: "); display.println(WIFI_SSID);
    display.print("Retry: "); display.println(retryCount);
    
    display.setCursor(0, 45);
    for(int i=0; i<(retryCount % 16); i++) display.print(".");
    display.display();

    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
    } else {
      delay(1000);
      retryCount++;
      if (retryCount % 30 == 0) {
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      }
    }
  }

  // 연결 성공 시 IP 표시
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("SYSTEM ONLINE");
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);
  display.setCursor(0, 25);
  display.println("IP ADDRESS:");
  display.setTextSize(2);
  display.println(WiFi.localIP().toString());
  display.display();
  
  Serial.println("\nWiFi Connected");
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  MDNS.begin("billboard");
  server.on("/", handleRoot);
  server.on("/update", handleUpdate);
  server.begin();
  
  delay(3000); 
}

// Function renaming to avoid naming collision in loop
void drawBlinkingText(String msg) {
    drawBlink(msg);
}

void loop() {
  server.handleClient();
  MDNS.update();

  // WiFi 연결 유지 확인 (끊기면 재부팅하여 복구)
  if (WiFi.status() != WL_CONNECTED) {
    display.clearDisplay();
    display.setCursor(0, 20);
    display.println("RECONNECTING...");
    display.display();
    delay(1000);
    ESP.restart(); 
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // 1. 전광판 애니메이션 렌더링
  switch (currentAnim) {
    case 0: drawStatic(currentMsg); break;
    case 1: drawScroll(currentMsg); break;
    case 2: drawBlinkingText(currentMsg); break;
    case 3: drawTypewriter(currentMsg); break;
    case 4: drawPulse(currentMsg); break;
    default: drawStatic(currentMsg); break;
  }

  // 2. 하단 정보 상태바
  display.drawFastHLine(0, 52, 128, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print("HTTP://");
  display.print(WiFi.localIP().toString());
  
  // 3. 루프 생존 표시기 (깜빡이는 점)
  if (millis() - statusMillis > 500) {
    statusDot = !statusDot;
    statusMillis = millis();
  }
  if (statusDot) display.fillCircle(124, 58, 2, SSD1306_WHITE);

  display.display();
  delay(20);
}
