/*--Import Libraries----------------------------------------------------------------------------------------------------------------*/
#include "Arduino.h"
#include "main.h"
#include "codec.h"
#include "config.h"
#include "comm.h"
/*--Create Params-------------------------------------------------------------------------------------------------------------------*/
const char* FW_VERSION      = "0001";                 // Firmware Verion, for OTA update

// Pinout & Params
const int TRIGPin           = 4 ;                     // HC-SR04 TRIG
  int TRIGState             = HIGH;                   //    Initial state (HIGH = TRIG LOW)
const int ECHOPin           = 5 ;                     // HC-SR04 ECHO
  unsigned long FBMicros    = 0;                      //    Feedback Micros() for ECHO rising edge
  unsigned long RISEMicros  = 0;                      //    RISE Micros() for ECHO falling edge
  long DURATION = 0;                                  //    Duration of the ECHO pulse
  signed long DISTANCE = 0;                           //    Distance predicted by ECHO
    signed long accDistance = 0;                      //      accumulatior for Distance rolling average
    int splDistance         = 0;                      //      accumulatior sampling
const int LEDPin            = 14 ;                    // 32xLEDs via NPNs BJT
  int LEDState              = 0;                      //    OUTPUT:0 = OFF; INPUT=ON
const int PIRPin            = 16 ;                    // AS312
  int PIRCount              = 0;                      //    Accumulated count [0:1024]
int sensorPin               = A0;                     // N/A
  int sensorValue           = 0;                      // N/A
  
// Variables
JsonObject statusjobject;                             // jObject for status for variable input
String statusjstring;                                 // jString for status for reply packet

unsigned long thisMillis, prevMillis;                 // store start mS of mainloop and a copy of it
unsigned long delayMillis;                            // delay of LED ON state in smart control

/*--Custom Functions----------------------------------------------------------------------------------------------------------------*/
void ECHOTRIGGER(){                                   // ECHO IOC
    FBMicros = micros();                              // store the micros 
    if (DURATION == 0){                               // Distance Check initiated
        if (digitalRead(ECHOPin) == LOW){             // ECHO = 0: end of check
            if (FBMicros >= RISEMicros)               // Avoid micros counter overflow
                DURATION = FBMicros - RISEMicros;     // compute the duration
        }else{                                        // ECHO = 1: start of check
            RISEMicros = FBMicros;                    // store Micros at rising edge
        }
    }
}

//  Rolling Average
//  Input:  &Accumulator, updated value, sampling (shift bit)
//  Output: Rolling Averaged value (& updated Accumulator)
signed long rollav(signed long* accval, signed long input, int spl){
    signed long delta = 0;                            // Prepare delta buffer
    signed long output = 0;                           // Prepare output buffer
    delta = input - (*accval >> spl);                 // Cumulative RT result for PWM
    *accval += delta;                                 // Update accumulative buffer
    output = *accval >> spl;                          // Save the ADC Result: 36 sample
    return output;                                    // 
}
/*--Default Setup()-----------------------------------------------------------------------------------------------------------------*/
void setup() {
    // Default Settings
    initSPIFFS();                                     // load SPIFFs and create Json params
    configuration();                                  // config the environments
    // Settings
    digitalWrite(12, LOW);                            // inital state
    digitalWrite(LEDPin, LOW);                        // LEDs: initial low
    pinMode(12, OUTPUT);                              // 
    pinMode(LEDPin, OUTPUT);                          // LEDs: output
    
    pinMode(PIRPin, INPUT);                           // PIR:  Input
    
    digitalWrite(TRIGPin, HIGH);                      // TRIG: initial low @ module
    pinMode(TRIGPin, OUTPUT);                         // TRIG: output
    pinMode(ECHOPin, INPUT);                          // ECHO: input
    attachInterrupt(ECHOPin, ECHOTRIGGER, CHANGE);    // ECHO IOC
    // Params    
    prevMillis = millis();
    thisMillis = millis();
}
/*--Default mainloop()--------------------------------------------------------------------------------------------------------------*/
void loop() {
    thisMillis = millis();                            // update millis() for the loop
    // WiFi Functions   
    if ((WiFiMulti.run() == WL_CONNECTED)) {
        if (message()){
        }else{
            otacheck();
        }
    }
//    // ADC
//    sensorValue = analogRead(A0);
    
    // ULTRASONIC: TRIG & ECHO
    splDistance = paramsjobject["smart"]["sonic"]["sampling"];
    if (DURATION > 0){                                // compute distance with valid duration only
        int tDISTANCE = DURATION / 58.2;              // mainloop is > 20mS >> Echo High Interval
        DISTANCE = rollav(&accDistance, tDISTANCE, splDistance);
    }    
    DURATION = 0;                                     // reset duration to allow ECHO check
    digitalWrite(TRIGPin, LOW);                       // set TRIG to 1
    delayMicroseconds(10);                            // spec: >10uS pulse
    digitalWrite(TRIGPin, HIGH);                      // end the pulse    
    // PIR
    if (
          (PIRCount > 0)
      and (digitalRead(PIRPin) == LOW)
      ){
        PIRCount--;
    }else if(
          (PIRCount < 1024)
      and (digitalRead(PIRPin) == HIGH)
      ){
        PIRCount++;
    }
//    Serial.println(PIRCount);
    
    // Delay Timer    
    if (prevMillis > thisMillis){                     // Avoid Internal Timer overflown
        unsigned long overflowMillis = 4294967295;    // prepare overflow count
        overflowMillis -= prevMillis;                 // compute the rest of count in previous Millis()
        if (delayMillis >= overflowMillis){           // Avoid operator overflown
            delayMillis -= overflowMillis;            // (-) that counts
        }else{                                        // Time is up
            delayMillis = 0;                          // set to 0
        }        
        if (delayMillis >= thisMillis){               // Avoid operator overflown
            delayMillis -= thisMillis;                // (-) that the Millis() of this loop
        }else{                                        // Time is up
            delayMillis = 0;                          // set to 0
        }
    }else if (delayMillis + prevMillis > thisMillis){ // Avoid operator overflown
        delayMillis += prevMillis - thisMillis;       // Compute delay based on difference of Millis()
    }else{                                            // Time is up
        delayMillis = 0;                              // set to 0
    }    
    if (    // Distance condition
            (DISTANCE < paramsjobject["smart"]["sonic"]["Don"])
            // PIR condition
        or  (PIRCount > paramsjobject["smart"]["pir"]["sampling"])
        ){
        delayMillis = paramsjobject["smart"]["delay"];// reset delay
        delayMillis = delayMillis * 1000;             // change delay from mS to Sec
    }
    
    // On / Off Decision
    if (paramsjobject["smart"]["on"] == 0){           // Smart Mode inactive
        LEDState = paramsjobject["on"];               // Manual On/Off
    }else{                                            // Smart Mode active
        if (delayMillis > 0){                         // Delay is active
            LEDState = 1;                             // LED On
        }else{                                        // No Delay
            LEDState = 0;                             // LED Off
        }
    }
    pinMode(LEDPin, !LEDState);                       // Update LED IO state
    pinMode(12,     !LEDState);                       // ..
    // Prepare Json & JString for communication
    DynamicJsonDocument statusjdoc;                   // Prepare jDoc to create status jObject
    statusjobject = statusjdoc.to<JsonObject>();      // assign jDoc to statusjObject
    statusjobject["on"]    = LEDState;                // load LED state
    statusjobject["delay"] = delayMillis;             // load Delay
statusjobject["delay"] = millis();
    
    statusjobject["sonic"] = DISTANCE;                // load Distance
    statusjobject["pir"]   = PIRCount;                // load PIR count
    statusjstring = "";                               // Empty previous jString
    serializeJson(statusjobject, statusjstring);      // write jString from jDoc with jObject
    prevMillis = thisMillis;                          // store to previous millis()
}
