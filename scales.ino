#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include "FS.h"
#include "hx711.h"

extern "C" {
#include "user_interface.h"
}

os_timer_t myTimer;
bool tickOccured;
int lastTime;
String JSON;
int lastMeasurement;

Hx711 scale(D2,D1);
ESP8266WebServer server(80);

const char* configSSID;       // name of network to make/connect to
const char* configPASSWORD;  // password for network
bool configTYPE;        // 0 for AP mode, 1 for CLIENT mode
int configRATIO;        // number to use for setScale(); used to divide hx711 output through
int configOFFSET;       // what is the value of getValue(); for an empty scale
bool configWARN;        // should we warn when fridge is empty ?
int configTHRESHOLD;    // below what getGram(); value should we warn ?
const char* configEMAIL;       // who should we send a mail to ?

void timerCallback(void *pArg) {

      tickOccured = true;
      Serial.println(lastMeasurement);

} // End of timerCallback

bool readConfig(String filename) {
  File jsonFile = SPIFFS.open(filename, "r");
  if (!jsonFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = jsonFile.size();
  if (size>1024) {
    Serial.println("JSON file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);


  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  jsonFile.readBytes(buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println("Failed to parse config file");
    return false;
  }

  jsonFile.close();
  
  configSSID = json["SSID"];
  configPASSWORD = json["PASSWORD"];
  configTYPE = json["TYPE"];
  configRATIO = json["RATIO"];
  configOFFSET = json["OFFSET"];
  configWARN = json["WARN"];
  configEMAIL = json["EMAIL"];
  
  return true; 
}


void handleJSON(){
  Serial.println("JSON called");
  JSON="{\"y\":";
  JSON+=lastMeasurement;
  JSON+="}";
  server.send(200,"application/json",JSON);
}


void setup() {
  Serial.begin(115200);
  Serial.println(); // clear screen
  Serial.println("Mounting FS...");

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }
  Serial.println ("FS mounted!");

  if (! readConfig("/config-techinc.json")) {
    Serial.println("Config Read FAIL\nHALTING\n");
    while (1) {}
  } else {
    Serial.println("---- Start of Config Parameters ----");
    Serial.print ("SSID: ");
    Serial.println (configSSID);
    Serial.print ("PASSWORD: ");
    Serial.println (configPASSWORD);
    Serial.print ("Apptype: ");
    if(configTYPE) {
      Serial.println("Client");
    }else{
      Serial.println("AP");
    }
    Serial.print ("Ratio: ");
    Serial.println (configRATIO);
    Serial.print ("Offset: ");
    Serial.println (configOFFSET);
    Serial.print ("Warn under threshold : ");
    if (configWARN) {
      Serial.println("Yes");
    }else{
      Serial.println("No");
    }
    Serial.print("Warn below: ");
    Serial.println(configTHRESHOLD);
    Serial.print("Email: ");
    Serial.println(configEMAIL);
    Serial.println("---- End of Config Parameters ----");
  }
  if (configTYPE) { // CLIENT MODE 
    Serial.println("Setting up WLAN in client-mode");
    WiFi.begin(configSSID,configPASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    
    Serial.println("");
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {          // AP-mode
    Serial.println("Setting up WLAN in AP-mode");
    WiFi.softAP(configSSID,configPASSWORD);
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());
  }

  Serial.println("Setting up timer for 60 seconds");
  tickOccured=false;
  os_timer_setfn(&myTimer, timerCallback, NULL);
  os_timer_arm(&myTimer, 5000, true);
  server.on("/json",handleJSON);
  server.serveStatic("/", SPIFFS, "/", "max-age=600");
  
  Serial.println("Setting  scale ratio");
  //scale.setScale(429); // Kitchenscale 
  //scale.setScale(22624);// Donar-scale; mate-bottles
  scale.setScale(11657); //Donar-scale; beer bottles
  scale.setOffset(scale.averageValue());
  
  server.begin(); 
  
}

void loop() {
 server.handleClient();

 if (tickOccured == true)
 {

    Serial.println("Tick Occurred");
    int timeNow=millis();
    Serial.print("Time: ");
    Serial.println(timeNow);
    Serial.print("Millis since last tick: ");
    Serial.println(timeNow-lastTime);
    lastMeasurement=scale.getGram();
    Serial.print("Measurement value: :");
    Serial.println(lastMeasurement);
    lastTime=timeNow;
    tickOccured = false;

 }
 
 yield();
}

