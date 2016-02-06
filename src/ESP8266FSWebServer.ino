/*
1)Access point wifi
2)Web server invio / ricezione pagine
3)File System con configurazioni e pagine html5+css
4)WebSockets server invio / ricezione dati
5)Captive portal

*/
#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Hash.h>

//7) WebSokets
#include <WebSocketsServer.h>
#include <Hash.h>
WebSocketsServer webSocket = WebSocketsServer(81);

//1)Access point Wifi
#include <ESP8266WiFi.h>

/* TODO: spostare in un file di configurazione nel file system? */
const char *ssid = "fablabparma";
const char *password = "fablabparma";
IPAddress apIP(192, 168, 100, 1);

//2)Web Server
#include <ESP8266WebServer.h>

ESP8266WebServer server(80);

//3)File system
#include <FS.h>

//6) DNS
#include <DNSServer.h>
const byte        DNS_PORT = 53;
DNSServer         dnsServer;

//mDNS
/* hostname for mDNS. Should work at least on windows. Try http://esp8266.local */
const char *myHostname = "esp8266";
#include <ESP8266mDNS.h>

//format bytes
String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

void setupAccessPoint(){
  //setup della modalità access point
  Serial.print("Configuring access point with SSID ");
  Serial.println(ssid);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid, password);

  IPAddress ip = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(ip);
  }

void printFSInfo(){
  FSInfo fsinfo;
  if(SPIFFS.info(fsinfo)){
    Serial.print("FS total bytes: ");
    Serial.println(fsinfo.totalBytes);
    Serial.print("FS used bytes: ");
    Serial.println(fsinfo.usedBytes);
  }
}

void setupFileSystem(){
  //file system
  if(!SPIFFS.begin()){
    Serial.println("Failed to mount file system");
  }
  else {
    Serial.println("File system mounted");
    printFSInfo();
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    Serial.printf("\n");
  }
}

void setupDNS(){
  //DNS
  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.start(DNS_PORT, "*", apIP);

  // Setup MDNS responder
  if (!MDNS.begin(myHostname)) {
    Serial.println("Error setting up MDNS responder!");
  } else {
    Serial.println("mDNS responder started");
    // Add service to MDNS-SD
    MDNS.addService("http", "tcp", 80);
  }
}

void handleRoot(){
  String response = "<html><head><script>var connection = new WebSocket('ws://'+location.hostname+':81/', ['arduino']);connection.onopen = function () {  connection.send('Connect ' + new Date()); }; connection.onerror = function (error) {    console.log('WebSocket Error ', error);};connection.onmessage = function (e) {  console.log('Server: ', e.data);};function sendRotation() {  var r = parseInt(document.getElementById('r').value).toString(); r = '#' + r;  console.log('Rotation: ' + r); connection.send(r); }</script></head>";
  response += "<body>Rotation Control:<br/><br/>R: <input id=\"r\" type=\"range\" min=\"0\" max=\"360\" step=\"1\" onchange=\"sendRotation();\" /><br/><br/></body></html>";
  
  server.send(200, "text/html", response);
}

void setupWebServer(){
  //server web
  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP server started");
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {

    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\r\n", num);
            break;
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("Client #[%u] connected from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);

            // send message to client
            webSocket.sendTXT(num, "Connected");
        }
            break;
        case WStype_TEXT:
            Serial.printf("Client #[%u] sent message: %s\r\n", num, payload);

            if(payload[0] == '#') {
                // we get rotation data
                // decode rotation data
                uint32_t rotation = (uint32_t) strtol((const char *) &payload[1], NULL, 16);

                Serial.printf("Get rotation value: [%u]\r\n", rotation);                
            }

            break;
    }

}

void setupWebSockets(){
  // start webSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);  
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println();

  setupAccessPoint();
  setupFileSystem();
  setupWebServer();
  setupDNS();
  setupWebSockets();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  webSocket.loop();
}
