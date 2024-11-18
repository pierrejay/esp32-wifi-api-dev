#include <Arduino.h>
#include <SPIFFS.h>
#include "WiFiManager.h"
#include "WiFiManagerAPI.h"
#include "APIServer.h"
#include "WebAPIEndpoint.h"

// Déclaration des objets globaux
WiFiManager wifiManager;  
APIServer apiServer;
WiFiManagerAPI wifiManagerAPI(wifiManager, apiServer);
WebAPIEndpoint webServer(apiServer, 80);  // Création sur la pile

void setup() {
    Serial.begin(115200);
    delay(3000);
    Serial.println("Démarrage...");

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    // Ajout du serveur web
    apiServer.addEndpoint(&webServer);  // On passe l'adresse

    // Vérification du système de fichiers
    if(!SPIFFS.begin(true)) {
        Serial.println("Erreur lors du montage SPIFFS");
        while(1) {
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
            delay(100);
        }
    }

    // Initialisation du WiFiManager
    if (!wifiManager.begin()) {
        Serial.println("Erreur d'initialisation WiFiManager");
        while(1) {
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
            delay(200);
        }
    }

    // Démarrage du serveur API
    apiServer.begin(); 

    Serial.println("Système initialisé");
    digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
    wifiManager.poll();
    wifiManagerAPI.poll();
    apiServer.poll(); 
}