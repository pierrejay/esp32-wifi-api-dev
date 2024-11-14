#include <Arduino.h>
#include "WiFiManager.h"
#include "APIServer.h"

// Déclaration des objets globaux
APIServer apiServer(80);  // Création du serveur sur le port 80
WiFiManager wifiManager(apiServer);  // Allocation statique, passage par référence

void setup() {
    // Initialisation de la communication série
    Serial.begin(115200);
    Serial.println("Démarrage...");

    // Initialisation du système de fichiers SPIFFS
    if(!SPIFFS.begin(true)) {
        Serial.println("Erreur lors du montage SPIFFS");
        return;
    }

    // Démarrage du serveur API
    apiServer.begin();

    Serial.println("Système initialisé");
}

void loop() {
    // Gestion des WebSockets
    apiServer.poll();

    // Autres tâches périodiques si nécessaire
    delay(10); // Petit délai pour éviter de surcharger le CPU
}