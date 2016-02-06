/*
Angelo Casalini 2016
*/

#include <Arduino.h>
#include <FS.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <Hash.h>

//WebSokets
WebSocketsServer webSocket = WebSocketsServer(81);

//Access point Wifi
/* TODO: spostare in un file di configurazione nel file system? */
const char *ssid = "fablabparma";
const char *password = "fablabparma";
IPAddress apIP(192, 168, 100, 1);

//Web Server
ESP8266WebServer server(80);

//DNS
const byte        DNS_PORT = 53;
DNSServer         dnsServer;

//mDNS
/* hostname for mDNS. Should work at least on windows. Try http://esp8266.local */
const char *myHostname = "esp8266";

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

String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

void handleFileList() {
  if(!server.hasArg("dir")) {server.send(500, "text/plain", "BAD ARGS"); return;}

  String path = server.arg("dir");
  Serial.println("handleFileList: " + path);
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while(dir.next()){
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir)?"dir":"file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }

  output += "]";
  server.send(200, "text/json", output);
}

bool handleFileRead(String path){
  Serial.println("handleFileRead: " + path);
  if(path.endsWith("/")) path += "index.html";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
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
    MDNS.addService("ws", "tcp", 81);
  }
}

void setupWebServer(){
  //list directory
  server.on("/list", HTTP_GET, handleFileList);
  //home page
  server.on("/", HTTP_GET, [](){
    if(!handleFileRead("/index.html")) server.send(404, "text/plain", "FileNotFound");
  });

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
  setupWebSockets();
  setupDNS();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  webSocket.loop();
}
