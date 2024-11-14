#include <Arduino.h>
#include "APIServer.h"
#include "WiFiManager.h"
#include "WiFiManagerAPI.h"
// Déclaration des objets globaux
APIServer apiServer(80);  // Création du serveur sur le port 80
WiFiManager wifiManager;  // Allocation statique, passage par référence
WiFiManagerAPI wifiManagerAPI(wifiManager, apiServer);

void setup() {
    // Initialisation de la communication série
    Serial.begin(115200);
    Serial.println("Démarrage...");

    digitalWrite(LED_BUILTIN, HIGH);

    // Initialisation du système de fichiers SPIFFS
    if(!SPIFFS.begin(true)) {
        Serial.println("Erreur lors du montage SPIFFS");
        return;
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