#include <Arduino.h>
#include <SPIFFS.h>
#include "WiFiManager.h"
#include "WiFiManagerAPI.h"
#include "APIServer.h"
#include "WebAPIEndpoint.h"

// Declaration of global objects
WiFiManager wifiManager;                                    // WiFiManager instance
APIServer apiServer;                                        // Master API server
WiFiManagerAPI wifiManagerAPI(wifiManager, apiServer);      // WiFiManager API interface
WebAPIEndpoint webServer(apiServer, 80);                    // Web server endpoint (HTTP+WS)

void setup() {
    Serial.begin(115200);
    Serial.println("Starting...");

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    // Add the web server endpoint to the API server
    apiServer.addEndpoint(&webServer);  // Pass the address

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