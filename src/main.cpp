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

#define endstopDownPin D5 // endstop for lowest capacitance
#define endstopUpPin   D6 // endstop for highest capacitance

// Wifi config
const char* ssid     = "yourAP";    // YOUR WIFI SSID
const char* password = "yourpw";    // YOUR WIFI PASSWORD 
const unsigned long endstopDebounce = 50;    // debounce time for endstops in ms

ESP8266WebServer  server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
MDNSResponder mdns;
int stepsRemaining = 0;
bool endstopDown = false; // lowest capacitance
bool endstopUp = false;   // highest capacitance
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
.stepbutton { border-radius: 35%; background-color: #f1f1f1; color: black; }
.buttons { width:5em; flex-grow:1; text-align: center; }
.status  { width:5em; flex-grow:1; text-align: center; }
.statusbubble  { border-radius: 100%; background-color: gray; width: 1.5em; height: 1.5em; margin: 0 auto; }
.good { background-color: #478847; }
.neutral { background-color: gray; }
.bad  { background-color: #bf3232; }
</style>
<script>
var websock;
function start() {
  websock = new WebSocket('ws://' + window.location.hostname + ':81/');
  websock.onopen = function(evt) { console.log('websock open'); document.querySelector("#connection").classList.replace("bad", "good"); };
  websock.onclose = function(evt) { console.log('websock close'); document.querySelector("#connection").classList.replace("good", "bad"); };
  websock.onerror = function(evt) { console.log(evt); };
  websock.onmessage = function(evt) { console.log(evt); handleEndstops(evt); };
}
function buttonclick(e) {
  websock.send(e.id);
}
function handleEndstops(e) {
  if(e.data == "endstop_up") {
  	document.querySelector("#endstop_up").classList.add("bad");
  }
  else if (e.data == "endstop_down") {
  	document.querySelector("#endstop_down").classList.add("bad");
  }
  else if (e.data == "endstop_ok") {
  	document.querySelector("#endstop_up").classList.remove("bad");
  	document.querySelector("#endstop_down").classList.remove("bad");
  }
}
</script>
</head>
<body onload="javascript:start();">
<h1 style="text-align: center;">MagLoop Tuner</h1>
<div id=container style="display:flex; justify-content: flex-end;">
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
<div id=container style="display:flex; justify-content: flex-end; margin-top: 3em">
	<div class="status">
		<p>endstop</p>
		<div id=endstop_down class="statusbubble neutral"></div>
		<p style="font-size:8pt">low cap</p>
	</div>
	<div class="status">
		<p>Connection</p>
		<div id=connection class="statusbubble bad"></div>
	</div>
	<div class="status">
		<p>endstop</p>
		<div id=endstop_up class="statusbubble neutral"></div>
		<p style="font-size:8pt">high cap</p>
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
  WiFi.begin(ssid, password);
  // Inform user we are trying to connect
  Serial.print(F("[ INFO ] Trying to connect WiFi: "));
  Serial.print(ssid);
  // We try it for 20 seconds and give up on if we can't connect
  unsigned long now = millis();
  uint8_t timeout = 20; // define when to time out in seconds
  // Wait until we connect or 20 seconds pass
  do {
    if (WiFi.status() == WL_CONNECTED) {
      break;
    }
    delay(500);
    Serial.print(F("."));
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
      if(endstopDown) {
        webSocket.broadcastTXT("endstop_down");
      }
      else if(endstopUp) {
        webSocket.broadcastTXT("endstop_up");
      }
      else {
        webSocket.broadcastTXT("endstop_ok");
      }
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
    endstopDown = !digitalRead(endstopDownPin);
    endstopUp   = !digitalRead(endstopUpPin);
    endstopPresstime = millis();
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("begin");
  delay(10);
  
  stepper.setRpm(10); // for max possible torque without overheating

  // setup endstops
  pinMode(endstopDownPin, INPUT_PULLUP);
  pinMode(endstopUpPin,   INPUT_PULLUP);
  
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
}

void loop()
{
  readEndstops();

  webSocket.loop();
  server.handleClient();
  
  // endstops reached, immediately stop for this direction
  if(endstopDown && stepsRemaining < 0) {
    Serial.println("low endstop reached");
    stepsRemaining = 0;
  }
  else if(endstopUp && stepsRemaining > 0) {
    Serial.println("high endstop reached");
    stepsRemaining = 0;
  }

  if(stepsRemaining) {
    bool moveClockWise = stepsRemaining < 0 ? false : true;
    stepper.step(moveClockWise);
    stepsRemaining = moveClockWise ? stepsRemaining - 1 : stepsRemaining + 1;
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