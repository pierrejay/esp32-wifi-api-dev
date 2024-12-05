#include <Arduino.h>
#include <SPIFFS.h>
#include "WiFiManager.h"
#include "WiFiManagerAPI.h"
#include "APIServer.h"
#include "WebAPIEndpoint.h"
#include "SerialAPIEndpoint.h"
#include "result.h"
#include "SerialProxy.h"
#include "APIDocGenerator.h"

#define GENERATE_API_DOC 0  // Mettre à 0 pour désactiver

// Proxy server for Serial port
// #define Serial SerialAPIEndpoint::proxy

// Declaration of global objects
WiFiManager wifiManager;                                    // WiFiManager instance
APIServer apiServer;                                        // APIServer instance                                       
WiFiManagerAPI wifiManagerAPI(wifiManager, apiServer);      // WiFiManager API interface
WebAPIEndpoint webServer(apiServer, 80);                    // Web server endpoint (HTTP+WS)
// SerialAPIEndpoint serialAPI(apiServer);                  // Serial API endpoint

void setup() {
    Serial.begin(115200);
    delay(5000);
    Serial.println("Starting...");

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    // Check the filesystem
    if(!SPIFFS.begin(true)) {
        Serial.println("Error mounting SPIFFS");
        while(1) {
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
            delay(100);
        }
    }

    //@API_DOC_SECTION_START
    // Register the API metadata
    APIInfo apiInfo;
    apiInfo.title = "WiFiManager API";
    apiInfo.version = "1.0.0";
    apiInfo.description = "WiFi operations control for ESP32";
    apiInfo.serverUrl = "http://esp32.local/api";
    apiInfo.license = "MIT";
    apiInfo.contact.name = "Pierre Jay";
    apiInfo.contact.email = "pierre.jay@gmail.com";
    apiServer.registerAPIInfo(apiInfo);
    //@API_DOC_SECTION_END

    //Add the web server endpoint to the API server
    apiServer.addEndpoint(&webServer);
    // apiServer.addEndpoint(&serialAPI);

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

    // Generate the API documentation
    if (GENERATE_API_DOC) {
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        if (APIDocGenerator::generateOpenAPIDocJson(apiServer, root, SPIFFS)) {
            Serial.println("OpenAPI documentation generated successfully");
        } else {
            Serial.println("Error generating OpenAPI documentation");
        }
    }
}

void loop() {
    // Poll the WiFiManager, its API interface and the API server
    wifiManager.poll();
    wifiManagerAPI.poll();
    apiServer.poll(); 
}