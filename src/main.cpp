#include <Arduino.h>
#include "APIServer.h"
#include "WiFiManager.h"
#include "WiFiManagerAPI.h"
#include <SPIFFS.h>

// Déclaration des objets globaux
WiFiManager wifiManager;  
APIServer apiServer(80);  
WiFiManagerAPI wifiManagerAPI(wifiManager, apiServer);

void setup() {
    Serial.begin(115200);
    delay(3000);
    Serial.println("Démarrage...");

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

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