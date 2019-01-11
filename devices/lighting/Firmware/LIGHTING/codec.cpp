#include "codec.h"

JsonObject updatejson(JsonObject oldjobject, JsonObject newjobject){  
    for (auto kv : newjobject) {                                          // loop via the new json jObject
        DynamicJsonDocument bufferjdoc;                                   // prepare jDoc for holding updated jObject
        DynamicJsonDocument updatejdoc;                                   // prepare jDoc for overwritting old jObject
        const char* key   = kv.key().c_str();                             // store the key of this loop
        JsonVariant jsonVariant = newjobject[key];                        // store jVariant of this loop
        if (jsonVariant.is<JsonArray>()){                                 // check if the jVariant is jArray
            int index = 0;                                                // initialize jArray loop index
            for (auto value : jsonVariant.as<JsonArray>()) {              // for every jVariant in array 
                JsonObject bufferjobject = updatejdoc.to<JsonObject>();   // create jObject to hold the updated jObject
                bufferjobject = updatejson(oldjobject[key][index], newjobject[key][index]); // recurrsive function: buffer to hold updated jObject
                JsonObject updatejobject = oldjobject[key][value];        // create jObject to hold old jObject at that position
                updatejobject = bufferjdoc.to<JsonObject>();              // assign bufferjdoc to the jObject
                updatejobject.copyFrom(bufferjobject);                    // perform copyFrom from buffer to that position
                index++;                                                  // increment for next jVariant
            }    
        }else if (jsonVariant.is<JsonObject>()){                          // check if the jVariant is jObject
            JsonObject bufferjobject = updatejson(oldjobject[key], newjobject[key]);  // recurrsive function: buffer to hold updated jObject
            JsonObject updatejobject = oldjobject[key];                   // create jObject to hold old jObject at that position
            updatejobject = bufferjdoc.to<JsonObject>();                  // assign bufferjdoc to the jObject
            updatejobject.copyFrom(bufferjobject);                        // perform copyFrom from buffer to that position
        }else{                                                            // Suppose jVariant is a value
            if (oldjobject.containsKey(key)){                             // Check if that level has the key
                JsonVariant oldjvar = oldjobject[key];                    // prepare jVariant of that position of old jObject for type check
                JsonVariant newjvar = newjobject[key];                    // prepare jVariant of that position of new jObject for type check
                if (oldjvar.is<int>() and newjvar.is<int>()){             // Check if both are integer
                    oldjobject[key] = kv.value().as<int>();               // allow to update the value
                }else if (oldjvar.is<const char*>() and newjvar.is<const char*>()){ // check if both are char*
                    oldjobject[key] = kv.value().as<char*>();             // allow to update the value
                }else{                                                    // type are mismatched
//                    Serial.print(F("Inconsistant incoming value..."));  // not allow to update
                }
            }else{                                                        // key is not found
//                Serial.print(F("key does not exists..."));              // not allow to add
            }
        }
    }    
    return oldjobject;                                                    // return the updated jObject to previous function
}
