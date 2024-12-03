#include <Arduino.h>
#include <SPIFFS.h>
#include "WiFiManager.h"
#include "WiFiManagerAPI.h"
#include "APIServer.h"
#include "WebAPIEndpoint.h"
#include "SerialAPIEndpoint.h"
#include "result.h"
#include "SerialProxy.h"

// Proxy server for Serial port
// #define Serial SerialAPIEndpoint::proxy

// Declaration of global objects
WiFiManager wifiManager;                                    // WiFiManager instance
APIServer apiServer({                                       // Master API server
    "WiFiManager API",                                      // title (required)
    "1.0.0"                                                 // version (required)
});                                       
WiFiManagerAPI wifiManagerAPI(wifiManager, apiServer);      // WiFiManager API interface
WebAPIEndpoint webServer(apiServer, 80);                    // Web server endpoint (HTTP+WS)
// SerialAPIEndpoint serialAPI(apiServer);                  // Serial API endpoint

void setup() {
    Serial.begin(115200);
    delay(5000);
    Serial.println("Starting...");

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    //Add the web server endpoint to the API server
    apiServer.addEndpoint(&webServer);
    // apiServer.addEndpoint(&serialAPI);

    // Define the optional API server metadata
    APIInfo& apiInfo = apiServer.getAPIInfo();
    apiInfo.description = "WiFi operations control for ESP32";
    apiInfo.serverUrl = "http://esp32.local/api";
    apiInfo.contact.name = "Pierre Jay";
    apiInfo.contact.email = "pierre.jay@gmail.com";
    apiInfo.license.name = "MIT";
    apiInfo.license.identifier = "MIT";

    // Check the filesystem
    if(!SPIFFS.begin(true)) {
        Serial.println("Error mounting SPIFFS");
        while(1) {
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
            delay(100);
        }
    }

    // Initialize the WiFiManager
    if (!wifiManager.begin()) {
        Serial.println("Error initializing WiFiManager");
        while(1) {
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
            delay(200);
        }
    }

    // Start the API server
    apiServer.begin(); 

    Serial.println("System initialized");
    digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
    // Poll the WiFiManager, its API interface and the API server
    wifiManager.poll();
    wifiManagerAPI.poll();
    apiServer.poll(); 
}