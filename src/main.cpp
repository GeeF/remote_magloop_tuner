#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <CheapStepper.h>

#define motorPin1 D0 // D5 <-> IN1 on the ULN2003 driver 1
#define motorPin2 D1 // D6 <-> IN2 on the ULN2003 driver 1
#define motorPin3 D2 // D7 <-> IN3 on the ULN2003 driver 1
#define motorPin4 D3 // D8 <-> IN4 on the ULN2003 driver 1

#define endstopPin D5 // endstop for lowest capacitance

#define maxSteps 5900 // max number of steps until stepper reaches upper end of the cap

// Wifi config
const char* ssid     = "myssid";    // YOUR WIFI SSID
const char* password = "mypw";    // YOUR WIFI PASSWORD 
const unsigned long endstopDebounce = 50;    // debounce time for endstops in ms

ESP8266WebServer  server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
MDNSResponder mdns;
int stepsRemaining = 0;
int32_t absolute_steps = 0;
bool endstop = false; // lowest capacitance
unsigned long endstopPresstime = 0;

CheapStepper stepper(motorPin1, motorPin2, motorPin3, motorPin4);

static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name = "viewport" content = "width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0">
<title>MagLoop Remote Tuner</title>
<style>
body { background-color: #ddd; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; margin: 0 auto; width: auto; max-width: 20em }
a { text-decoration: none; display: inline-block; padding: 8px 16px; font-size: 25pt}
a:hover { background-color: #ccc; color: black; }
#tune_canvas { margin: 3em auto 1em auto; display: block; border:2px solid #000000; background-color: #aca }
.container { display:flex; justify-content: flex-end; }
.stepbutton { border-radius: 35%; background-color: #f1f1f1; margin-top: -.3em; margin-bottom: -.3em; color: black; }
.buttons { width:5em; flex-grow:1; text-align: center; }
.status  { width:5em; flex-grow:1; text-align: center; }
.statusbubble  { border-radius: 100%; background-color: gray; width: 1.5em; height: 1.5em; margin: -.5em auto; }
.good { background-color: #478847; }
.neutral { background-color: gray; }
.bad  { background-color: #bf3232; }
</style>
<script>
var websock;
var maxSteps = 5900; // whole cap is 5900 steps
var pos80m   = 1800; // rough pos of 80m tune area
var pos40m   = 900;  // rough pos of 80m tune area
var pos30m   = 650;  // rough pos of 80m tune area
var pos20m   = 450;  // rough pos of 80m tune area
var pos10m   = 200;  // rough pos of 80m tune area

function start() {
  websock = new WebSocket('ws://' + window.location.hostname + ':81/');
  websock.onopen = function(evt) { 
    console.log('websock open');
    document.querySelector("#connection").classList.replace("bad", "good");
    drawBands(); 
  };
  websock.onclose = function(evt) { console.log('websock close'); document.querySelector("#connection").classList.replace("good", "bad"); };
  websock.onerror = function(evt) { console.log(evt); };
  websock.onmessage = function(evt) { console.log(evt); handleMessages(evt); };
}
function buttonclick(e) {
  websock.send(e.id);
}
function handleMessages(e) {
  if(e.data == "endstop") {
    document.querySelector("#endstop").classList.add("bad");
  }
  else if (e.data == "endstop_ok") {
    document.querySelector("#endstop").classList.remove("bad");
  }
  else if (e.data.startsWith("absolute_steps")) {
    var absolute_steps = parseInt(e.data.split(" ")[1], 10);
    updateCanvas(absolute_steps);
  }
}
function drawBands() {
  var canvas = document.getElementById("tune_canvas");
  var ctx = canvas.getContext("2d");
  // draw rough estimates for the bands
  ctx.font = "10px sans-serif";
  ctx.fillText("80m", pos80m / maxSteps * canvas.width, 45);
  ctx.fillText("40m", pos40m / maxSteps * canvas.width, 45);
  ctx.fillText("30m", pos30m / maxSteps * canvas.width, 45);
  ctx.fillText("20m", pos20m / maxSteps * canvas.width, 45);
  ctx.fillText("10m", pos10m / maxSteps * canvas.width, 45);
}
function updateCanvas(v) {
  var canvas = document.getElementById("tune_canvas");
  var ctx = canvas.getContext("2d");
  var needle_pos = v / maxSteps * canvas.width;
  ctx.clearRect(0, 0, canvas.width, canvas.height);

  drawBands();

  ctx.beginPath();
  ctx.lineWidth = 3;
  ctx.strokeStyle = "#a44";
  ctx.moveTo(needle_pos, 0);
  ctx.lineTo(needle_pos, 50);
  ctx.stroke();
}
</script>
</head>
<body onload="javascript:start();">
<h1 style="text-align: center;">MagLoop Tuner</h1>
<div class="container">
    <div class="buttons">
    <a href="#" id="up_1"   class="stepbutton" onclick="buttonclick(this);">&uArr;</a>
    <p>1</p>
    <a href="#" id="down_1" class="stepbutton" onclick="buttonclick(this);">&dArr;</a>
    </div>
    <div class="buttons">
    <a href="#" id="up_10"   class="stepbutton" onclick="buttonclick(this);">&uArr;</a>
    <p>10</p>
    <a href="#" id="down_10" class="stepbutton" onclick="buttonclick(this);">&dArr;</a>
    </div>
    <div class="buttons">
    <a href="#" id="up_100"   class="stepbutton" onclick="buttonclick(this);">&uArr;</a>
    <p>100</p>
    <a href="#" id="down_100" class="stepbutton" onclick="buttonclick(this);">&dArr;</a>
    </div>
</div>
<canvas id="tune_canvas" width="250" height="50"></canvas> 
<div class="container">
    <div class="status">
        <p>endstop</p>
        <div id=endstop class="statusbubble neutral"></div>
    </div>
    <div class="status">
        <p>Connection</p>
        <div id=connection class="statusbubble bad"></div>
    </div>
</div>)rawliteral";

void fallbacktoAPMode() {
  Serial.println(F("[ INFO ] ESP-RFID is running in Fallback AP Mode"));
  uint8_t macAddr[6];
  WiFi.softAPmacAddress(macAddr);
  char ssid[15];
  sprintf(ssid, "ESP-RFID-%02x%02x%02x", macAddr[3], macAddr[4], macAddr[5]);
  
  WiFi.mode(WIFI_AP);
  Serial.print(F("[ INFO ] Configuring access point... "));
  bool success = WiFi.softAP(ssid, password);
  Serial.println(success ? "Ready" : "Failed!");
  // Access Point IP
  IPAddress myIP = WiFi.softAPIP();
  Serial.print(F("[ INFO ] AP IP address: "));
  Serial.println(myIP);
  Serial.printf("[ INFO ] AP SSID: %s\n", ssid);
}

// Try to connect Wi-Fi
bool connectSTA(const char* ssid, const char* password) {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  // Wifi scan
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
  }
  Serial.println("");

  // First connect to a wi-fi network
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  // Inform user we are trying to connect
  Serial.print(F("[ INFO ] Trying to connect to WiFi: "));
  Serial.print(ssid);
  // We try it for 20 seconds and give up on if we can't connect
  unsigned long now = millis();
  uint8_t timeout = 120; // define when to time out in seconds
  // Wait until we connect or 20 seconds pass
  do {
    if (WiFi.status() == WL_CONNECTED) {
      break;
    }
    delay(500);
    Serial.print(F("."));
    if(WiFi.status() == 1) { // ssid lost
      WiFi.disconnect(true);
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);
      Serial.println(F("[ INFO ] retrying connect..."));
    }
  }
  while (millis() - now < timeout * 1000);
  // We now out of the while loop, either time is out or we connected. check what happened
  if (WiFi.status() == WL_CONNECTED) { // Assume time is out first and check
    Serial.println();
    Serial.print(F("[ INFO ] Client IP address: ")); // Great, we connected, inform
    Serial.println(WiFi.localIP());
    return true;
  }
  else { // We couln't connect, time is out, inform
    Serial.println();
    Serial.println(F("[ WARN ] Couldn't connect in time"));
    return false;
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length)
{
  Serial.printf("webSocketEvent(%d, %d, ...)\r\n", num, type);
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\r\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        // send absolute steps initially
        webSocket.broadcastTXT(String("absolute_steps ") + String(absolute_steps));
      }
      break;
    case WStype_TEXT:
      Serial.printf("[%u] got Text: %s\r\n", num, payload);

      if (strcmp("up_1", (const char *)payload) == 0) {
        stepsRemaining += 1;
      }
      else if (strcmp("down_1", (const char *)payload) == 0) {
        stepsRemaining -= 1;
      }
      else if (strcmp("up_10", (const char *)payload) == 0) {
        stepsRemaining += 10;
      }
      else if (strcmp("down_10", (const char *)payload) == 0) {
        stepsRemaining -= 10;
      }
      else if (strcmp("up_100", (const char *)payload) == 0) {
        stepsRemaining += 100;
      }
      else if (strcmp("down_100", (const char *)payload) == 0) {
        stepsRemaining -= 100;
      }
      else {
        Serial.println("Unknown command");
      }
      // send endstop status data to all connected clients
      if(endstop) {
        webSocket.broadcastTXT("endstop");
      }
      else {
        webSocket.broadcastTXT("endstop_ok");
      }
      webSocket.broadcastTXT(String("absolute_steps ") + String(absolute_steps));
      break;
    case WStype_BIN:
      Serial.printf("[%u] get binary length: %u\r\n", num, length);
      hexdump(payload, length);

      // echo data back to browser
      webSocket.sendBIN(num, payload, length);
      break;
    default:
      Serial.printf("Invalid WStype [%d]\r\n", type);
      break;
  }
}

void handleRoot()
{
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void init_mdns() {
  if (mdns.begin("maglooptuner", WiFi.localIP())) {
    Serial.println("MDNS responder started");
    mdns.addService("http", "tcp", 80);
    mdns.addService("ws", "tcp", 81);
  }
  else {
    Serial.println("MDNS.begin failed");
  }
  Serial.println("Connect to http://maglooptuner.local or http://");
}

void readEndstops()
{
  if(millis() - endstopPresstime > endstopDebounce) {
    endstop = !digitalRead(endstopPin);
    endstopPresstime = millis();
  }
}

// home stepper and move back to initial position to get absolute_step number
void home() {
  uint32_t steps_to_move_back = 0;
  Serial.println("homing...");

  while(!endstop) {
    readEndstops();
    stepper.step(false); // move anti-clockwise
    yield(); // prevent wdt soft reset
    steps_to_move_back++;
  }
  Serial.println("low endstop reached. moving back");
  stepsRemaining = steps_to_move_back;
}

void setup()
{
  Serial.begin(115200);
  Serial.println("begin");
  delay(10);
  
  stepper.setRpm(10); // for max possible torque without overheating

  // setup endstop
  pinMode(endstopPin, INPUT_PULLUP);
  
  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("RemoteTuner: Connecting to ");
  Serial.println(ssid);

  if(!connectSTA(ssid, password)) fallbacktoAPMode();

  init_mdns();

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);

  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  Serial.println("WiFi connected");

  home();
}

void loop()
{
  readEndstops();

  webSocket.loop();
  server.handleClient();
  
  // endstops reached, immediately stop for this direction
  if(endstop && stepsRemaining < 0) {
    Serial.println("low endstop reached");
    stepsRemaining = 0;
  }

  if(absolute_steps >= maxSteps && stepsRemaining > 0) {
    Serial.println("max steps reached");
    stepsRemaining = 0;
  }

  if(stepsRemaining) {
    bool moveClockWise = stepsRemaining < 0 ? false : true;
    stepper.step(moveClockWise);
    stepsRemaining = moveClockWise ? stepsRemaining - 1 : stepsRemaining + 1;
    absolute_steps = moveClockWise ? absolute_steps + 1 : absolute_steps - 1;
    //Serial.println("Steps remaining: " + String(stepsRemaining));
  }
  else {
    // turn off stepper pins to reduce RFI and power consumption. We don't need holding torque
    digitalWrite(motorPin1, LOW);
    digitalWrite(motorPin2, LOW);
    digitalWrite(motorPin3, LOW);
    digitalWrite(motorPin4, LOW);
  }
}