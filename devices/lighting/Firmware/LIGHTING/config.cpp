#include "main.h"
#include "codec.h"
#include "config.h"

#include "FS.h"
#include <WiFiClient.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

ESP8266WiFiMulti WiFiMulti;
WiFiServer server(80);

DynamicJsonDocument configjdoc;
JsonObject configjobject;
String configjstring;

DynamicJsonDocument paramsjdoc;
JsonObject paramsjobject;
String paramsjstring;

char fwUrlBase    [50];
char fwVersionURL [50];
char fwLoginID    [20];
char fwLoginPW    [20];

String oldFWVersion;
String newFWVersion;

bool initSPIFFS(){
//    Serial.println(F("initialize Params..."));    
    if (!SPIFFS.begin()) {
//        Serial.println(F("Failed to mount file system"));
        return false;
    }
    
//    Serial.println(F("opening config file"));
    File configFile = SPIFFS.open("/config.json", "r");
    if (!configFile) {
//        Serial.println(F("Failed to open config file"));
        return false;
    }
  
    // Allocate a buffer to store contents of the file.
//    DynamicJsonDocument configjdoc;
    configjstring = configFile.readString();
    deserializeJson(configjdoc, configjstring);
    configjobject = configjdoc.as<JsonObject>();
    
    Serial.println(F("opening params file"));
    File paramsFile = SPIFFS.open("/params.json", "r");
    if (!paramsFile) {
//        Serial.println(F("Failed to open params file"));
        return false;
    }
  
    // Allocate a buffer to store contents of the file.
//    DynamicJsonDocument paramsjdoc;
    paramsjstring = paramsFile.readString();
    deserializeJson(paramsjdoc, paramsjstring);
    paramsjobject = paramsjdoc.as<JsonObject>();
    
    return true;    
}

void configuration() {
    ESP.wdtEnable(WDTO_8S);       // Initialize Software Watchdog
    Serial.begin(115200);         // Start the Serial communication to send messages to the computer
    delay(10);
    const String  type    = configjobject["type"];
    JsonArray     wifi    = configjobject["wifi"];  
    const char*   otahost = configjobject["ota"]["host"];
    const int     otaport = configjobject["ota"]["port"];
    const char*   otaid   = configjobject["ota"]["id"];
    const char*   otapw   = configjobject["ota"]["pw"]; 

    oldFWVersion = type + FW_VERSION;
    oldFWVersion.concat( ".bin" );
    newFWVersion = oldFWVersion;

    Serial.println(newFWVersion);
    
    for (int i = 0; i < wifi.size(); i++){
        const char* ssid      = wifi[i]["ssid"];
        const char* password  = wifi[i]["password"];
        WiFiMulti.addAP(ssid, password);   // add Wi-Fi networks you want to connect to
    }
    sprintf(fwUrlBase,    "%s%s", otahost, F("/OTA/"));
    sprintf(fwVersionURL, "%s%s", otahost, F("/OTA.php"));
    sprintf(fwLoginID,    "%s",   otaid);
    sprintf(fwLoginPW,    "%s",   otapw);
    int i = 0;
    while (WiFiMulti.run() != WL_CONNECTED) { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
        delay(1000);
    }
    Serial.print(F("Connected to "));
    Serial.println(WiFi.SSID());              // Tell us what network we're connected to
    Serial.print(F("IP address:\t"));
    Serial.println(WiFi.localIP());           // Send the IP address of the ESP8266 to the computer
    server.begin();                             // starts the TCP/IP Server
}

void otacheck(){
  
    WiFiClient fwclient;
    HTTPClient httpClient;
    
    if (httpClient.begin(fwclient, fwVersionURL)) {             // HTTP
        httpClient.setAuthorization(fwLoginID, fwLoginPW);      // Server Login
        // start connection and send HTTP header
        httpClient.addHeader("Content-Type","text/plain");
        int httpCode = httpClient.GET();
        if( httpCode == 200 ) {
            newFWVersion = httpClient.getString();
            if( oldFWVersion != newFWVersion) {
                WiFiClient fwclient;
                String fwImageURL = fwUrlBase;
                fwImageURL.concat(newFWVersion);
                Serial.print (fwImageURL);
                t_httpUpdate_return ret = ESPhttpUpdate.update(fwclient, fwImageURL);                  
                switch(ret) {
                    case HTTP_UPDATE_FAILED:
                        Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                        break;
                    case HTTP_UPDATE_NO_UPDATES:
                        Serial.println("HTTP_UPDATE_NO_UPDATES");
                        break;                      
                    case HTTP_UPDATE_OK:
                        Serial.println("HTTP_UPDATE_OK");
                        break;
                }
            }
        }else {
            Serial.print( F("Firmware version check failed, got HTTP response code ") );
            Serial.println( httpCode );
        }
    }
    httpClient.end();
}
