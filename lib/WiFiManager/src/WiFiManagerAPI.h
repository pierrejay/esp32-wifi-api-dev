#ifndef WIFI_MANAGER_API_H
#define WIFI_MANAGER_API_H

#include <ESPAsyncWebServer.h>
#include <APIServer.h>
#include <ArduinoJson.h>
#include "WiFiManager.h"

constexpr unsigned long NOTIFICATION_INTERVAL = 500; // Intervalle minimum entre les mises à jour WebSocket en millisecondes

/**
 * @brief Classe API pour le gestionnaire WiFi
 * 
 * Cette classe fournit une interface REST API et WebSocket pour le gestionnaire WiFi.
 * Elle gère les requêtes HTTP pour obtenir et définir les paramètres WiFi,
 * et diffuse les changements d'état via WebSocket.
 */
class WiFiManagerAPI {
public:
    /**
     * @brief Construit un nouvel objet WiFiManagerAPI
     * 
     * @param wifiManager Référence vers l'instance du gestionnaire WiFi
     * @param apiServer Référence vers le serveur API pour la gestion WebSocket
     */
    WiFiManagerAPI(WiFiManager& wifiManager, APIServer& apiServer) 
        : _wifiManager(wifiManager), _apiServer(apiServer), _lastNotification(0) {
        
        // S'abonner aux changements d'état
        _wifiManager.onStateChange([this]() {
            sendNotification(true);
        });
        
        registerMethods();
    }

    /**
     * @brief Vérifie les changements d'état et envoie les mises à jour WebSocket
     * 
     * Doit être appelé régulièrement dans la boucle principale.
     * Envoie les mises à jour via WebSocket lorsque des changements d'état sont détectés
     * et que l'intervalle minimum est écoulé.
     */
    void poll() {
        unsigned long now = millis();
        if (now - _lastNotification > NOTIFICATION_INTERVAL) {
            if (sendNotification(false)) _lastNotification = now;
        }
    }

private:
    WiFiManager& _wifiManager;
    APIServer& _apiServer;
    unsigned long _lastNotification;
    StaticJsonDocument<2048> _previousState;

    /**
     * @brief Envoie les mises à jour d'état via WebSocket
     * 
     * @param force Force l'envoi même si aucun changement n'est détecté
     * @return true si des changements ont été envoyés
     */
    bool sendNotification(bool force = false) {
        // Fetch the current status and config
        StaticJsonDocument<2048> newState;
        JsonObject newStatus = newState["status"].to<JsonObject>();
        JsonObject newConfig = newState["config"].to<JsonObject>();
        _wifiManager.getStatusToJson(newStatus);
        _wifiManager.getConfigToJson(newConfig);

        // Check if there are changes (or force update)
        bool changed = (force || _previousState.isNull() || newState != _previousState);
        
        if (changed) {
            StaticJsonDocument<2048> sendDoc;
            sendDoc["type"] = "wifi_manager";
            JsonObject data = sendDoc["data"].to<JsonObject>();
            data["status"] = newStatus;
            data["config"] = newConfig;
            String message;
            serializeJson(sendDoc, message);
            _apiServer.push(message);
            _previousState = newState;
            return true;    
        }
        return false;
    }

    /**
     * @brief Enregistre les méthodes API
     */
    void registerMethods() {
        // Enregistrer les méthodes GET
        _apiServer.addGetMethod("wifi", "status", [this](const JsonObject* args, JsonObject& response) {
            _wifiManager.getStatusToJson(response);
            return true;
        });

        _apiServer.addGetMethod("wifi", "config", [this](const JsonObject* args, JsonObject& response) {
            response["hostname"] = _wifiManager.getHostname();
            JsonObject config = response["config"].to<JsonObject>();
            _wifiManager.getConfigToJson(config);
            return true;
        });

        _apiServer.addGetMethod("wifi", "scan", [this](const JsonObject* args, JsonObject& response) {
            _wifiManager.getAvailableNetworks(response);
            return true;
        });

        // Enregistrer les méthodes SET
        _apiServer.addSetMethod("wifi", "ap/config", [this](const JsonObject* args, JsonObject& response) {
            bool success = _wifiManager.setAPConfigFromJson(*args);
            response["success"] = success;
            return true;
        });

        _apiServer.addSetMethod("wifi", "sta/config", [this](const JsonObject* args, JsonObject& response) {
            bool success = _wifiManager.setSTAConfigFromJson(*args);
            response["success"] = success;
            return true;
        });

        _apiServer.addSetMethod("wifi", "hostname", [this](const JsonObject* args, JsonObject& response) {
            if (!(*args)["hostname"].is<const char*>()) {
                return false;
            }
            bool success = _wifiManager.setHostname((*args)["hostname"].as<const char*>());
            response["success"] = success;
            return true;
        });
    }
};

#endif // WIFI_MANAGER_API_H 