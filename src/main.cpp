#include <Arduino.h>
#include <SPIFFS.h>
#include "WiFiManager.h"
#include "WiFiManagerAPI.h"
#include "RPCServer.h"
#include "WebAPIServer.h"

// Déclaration des objets globaux
WiFiManager wifiManager;  
RPCServer rpcServer;
WiFiManagerAPI wifiManagerAPI(wifiManager, rpcServer);
WebAPIServer webServer(rpcServer, 80);  // Création sur la pile

void setup() {
    Serial.begin(115200);
    delay(3000);
    Serial.println("Démarrage...");

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    // Ajout du serveur web
    rpcServer.addServer(&webServer);  // On passe l'adresse

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

    // Démarrage du serveur RPC
    rpcServer.begin();  // Changé de apiServer à rpcServer

    Serial.println("Système initialisé");
    digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
    wifiManager.poll();
    wifiManagerAPI.poll();
    rpcServer.poll();  // Changé de apiServer à rpcServer
}