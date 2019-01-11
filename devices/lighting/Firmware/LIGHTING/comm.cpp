#include <ctype.h>
#include "codec.h"
#include "config.h"
#include "main.h"
#include "comm.h"

#include <WiFiClient.h>
#include "FS.h"

JsonObject commjobject;
String commjstring;

bool message(){
    WiFiClient client = server.available();
    if (client) {
        if (client.connected()) {
            commjstring = client.readStringUntil('\r');       // receives the message from the client

            Serial.print(F("Message Received: "));
            Serial.println(commjstring);
            
            // Create commjobject
            DynamicJsonDocument commjdoc;                   
            deserializeJson(commjdoc, commjstring);
            commjobject = commjdoc.as<JsonObject>();
            // Prepare the reply
            client.flush();
            // What to do? loop via the request message
            for (auto kv : commjobject) {
                // Based on json key and key size at root
                int len = JsonObject(commjobject[kv.key().c_str()]).size();
                // report if key size is 0, edit it otherwise
                if (String(kv.key().c_str()) == "config"){
//                    Serial.println(F("config message..."));
                    if (len > 0){        
//                        Serial.println(F("update config..."));
                        configjobject = updatejson(configjobject, commjobject["config"]);
                        // Update json string
                        configjstring = "";
                        serializeJson(configjobject, configjstring);                        
                        // Update config.json in SPIFFS
                        File configFile = SPIFFS.open("/config.json", "w");
                        if (!configFile) {
                            Serial.println(F("Failed to open config file for writing"));
                            return false;
                        }
                        serializeJson(configjobject, configFile);
                        serializeJson(configjobject, Serial);
                        Serial.println();
                    }
                    // Reply to Host
                    client.println(configjstring);
                }else if (String(kv.key().c_str()) == "params"){
//                    Serial.println(F("params message..."));
                    if (len > 0){          
                        paramsjobject = updatejson(paramsjobject, commjobject["params"]);
                        // Update json string
                        paramsjstring = "";
                        serializeJson(paramsjobject, paramsjstring);
                        // Update params.json in SPIFFS
                        File paramsFile = SPIFFS.open("/params.json", "w");
                        if (!paramsFile) {
                            Serial.println(F("Failed to open params file for writing"));
                            return false;
                        }
                        serializeJson(paramsjobject, paramsFile);
                        serializeJson(paramsjobject, Serial);
                        Serial.println();
                    }
                    // Reply to Host
                    client.println(paramsjstring);
                }else{
                    // Reply to Host
                    client.println(statusjstring);
                }
            }                       
            client.stop();                                          // tarminates the connection with the client
            return true;
        }
        client.stop();                                          // tarminates the connection with the client
    }
     return false;
}
