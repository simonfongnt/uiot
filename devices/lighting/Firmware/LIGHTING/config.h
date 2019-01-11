#ifndef config_h
#define config_h
#include "Arduino.h"
#include <ArduinoJson.h>

#include <ESP8266WiFiMulti.h>

extern ESP8266WiFiMulti WiFiMulti;
extern WiFiServer server;

extern JsonObject configjobject;
extern String configjstring;

extern JsonObject paramsjobject;
extern String paramsjstring;

bool initSPIFFS();
void configuration();
void otacheck();

#endif
