#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 14
#define OLED_SCL 12

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
ESP8266WebServer server(80);

String currentMsg = "Enter Text in Web Side";
int currentAnim = 1; 
unsigned long lastUpdate = 0;
int xPos = SCREEN_WIDTH;
bool blinkState = true;
int charIndex = 0;
float scale = 1.0;
bool scaleUp = true;

const char* HTML_CONTENT = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1.0'><title>Billboard</title>
<style>
body{background:#0a0b1e;color:#00d4ff;font-family:sans-serif;text-align:center;padding:20px;margin:0}
.box{background:rgba(255,255,255,0.1);padding:30px;border-radius:20px;margin:20px auto;max-width:400px}
input,select,button{width:100%;padding:15px;margin:10px 0;border-radius:10px;border:none;font-size:16px;box-sizing:border-box}
button{background:#00d4ff;color:#000;font-weight:bold;cursor:pointer}
</style></head>
<body><div class='box'><h1>MINI BILLBOARD</h1>
<input type='text' id='m' placeholder='Enter Message' maxlength='32'>
<select id='a'><option value='0'>STATIC</option><option value='1'>SCROLL</option><option value='2'>BLINK</option><option value='3'>TYPEWRITER</option><option value='4'>PULSE</option></select>
<button onclick='s()'>SEND TO DISPLAY</button></div>
<script>function s(){const m=document.getElementById('m').value;const a=document.getElementById('a').value;fetch('/u?m='+encodeURIComponent(m)+'&a='+a).then(()=>alert('Sent!'))}</script>
</body></html>)rawliteral";

void handleRoot() {
  Serial.println("[SERVER] Root page requested");
  server.send(200, "text/html", HTML_CONTENT);
}

void handleUpdate() {
  Serial.println("[SERVER] Update requested");
  if(server.hasArg("m")) {
    currentMsg = server.arg("m");
    if(currentMsg == "") currentMsg = "Empty";
  }
  if(server.hasArg("a")) currentAnim = server.arg("a").toInt();
  
  xPos = SCREEN_WIDTH; charIndex = 0; scale = 1.0;
  server.send(200, "text/plain", "OK");
  Serial.print("New Msg: "); Serial.println(currentMsg);
}

void drawStatic(String msg) { display.setTextSize(2); int16_t x,y; uint16_t w,h; display.getTextBounds(msg,0,0,&x,&y,&w,&h); display.setCursor((128-w)/2,(52-h)/2); display.print(msg); }
void drawScroll(String msg) { display.setTextSize(2); int16_t x,y; uint16_t w,h; display.getTextBounds(msg,0,0,&x,&y,&w,&h); if(millis()-lastUpdate>30){xPos-=2;if(xPos<-(int)w)xPos=128;lastUpdate=millis();} display.setCursor(xPos,(52-h)/2); display.print(msg); }
void drawBlink(String msg) { display.setTextSize(2); if(millis()-lastUpdate>500){blinkState=!blinkState;lastUpdate=millis();} if(blinkState){ int16_t x,y; uint16_t w,h; display.getTextBounds(msg,0,0,&x,&y,&w,&h); display.setCursor((128-w)/2,(52-h)/2); display.print(msg); } }
void drawTypewriter(String msg) { display.setTextSize(2); if(millis()-lastUpdate>150){charIndex++;if(charIndex>(int)msg.length()){if(millis()-lastUpdate>2000)charIndex=0;}else{lastUpdate=millis();}} String sub=msg.substring(0,charIndex); int16_t x,y; uint16_t w,h; display.getTextBounds(sub,0,0,&x,&y,&w,&h); display.setCursor((128-w)/2,(52-h)/2); display.print(sub); if(millis()%1000<500)display.print("_"); }
void drawPulse(String msg) { if(millis()-lastUpdate>40){if(scaleUp)scale+=0.05;else scale-=0.05;if(scale>1.3)scaleUp=false;if(scale<0.7)scaleUp=true;lastUpdate=millis();} display.setTextSize((scale>1.1)?3:2); int16_t x,y; uint16_t w,h; display.getTextBounds(msg,0,0,&x,&y,&w,&h); display.setCursor((128-w)/2,(52-h)/2); display.print(msg); }

void setup() {
  Serial.begin(74880);
  Serial.println("\n\n-- System Boot --");
  
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Connecting WiFi...");
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 40) {
    delay(500); Serial.print(".");
    display.print("."); display.display();
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected");
    Serial.print("IP Address: "); Serial.println(WiFi.localIP());
    
    MDNS.begin("billboard");
    server.on("/", handleRoot);
    server.on("/u", handleUpdate);
    server.begin();
    Serial.println("HTTP Server Started");
    
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("STA CONNECTED!");
    display.println("");
    display.println("Visit Address:");
    display.println(WiFi.localIP().toString());
    display.display();
    delay(3000);
  }
}

void loop() {
  server.handleClient();
  MDNS.update();
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  switch (currentAnim) {
    case 0: drawStatic(currentMsg); break;
    case 1: drawScroll(currentMsg); break;
    case 2: drawBlink(currentMsg); break;
    case 3: drawTypewriter(currentMsg); break;
    case 4: drawPulse(currentMsg); break;
  }
  
  display.drawFastHLine(0, 52, 128, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print("HTTP://"); display.print(WiFi.localIP().toString());
  
  display.display();
  yield(); // WiFi 처리를 위한 양보
}
