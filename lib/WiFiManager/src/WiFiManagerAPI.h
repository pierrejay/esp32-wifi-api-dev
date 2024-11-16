#ifndef WIFI_MANAGER_API_H
#define WIFI_MANAGER_API_H

#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <APIServer.h>
#include <ArduinoJson.h>
#include "WiFiManager.h"

constexpr unsigned long WS_SEND_INTERVAL = 500; // Intervalle minimum entre les mises à jour WebSocket en millisecondes

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
        : _wifiManager(wifiManager), _apiServer(apiServer), _lastWsUpdate(0) {
        
        // S'abonner aux changements d'état
        _wifiManager.onStateChange([this]() {
            sendWsUpdates(true);
        });
        
        // Obtenir une référence au serveur web
        AsyncWebServer& server = apiServer.server();
        registerRoutes(server);
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
        if (now - _lastWsUpdate > WS_SEND_INTERVAL) {
            if (sendWsUpdates(false)) _lastWsUpdate = now;
        }
    }

private:
    WiFiManager& _wifiManager;
    APIServer& _apiServer;
    unsigned long _lastWsUpdate;
    StaticJsonDocument<2048> _previousState;

    /**
     * @brief Enregistre toutes les routes API
     * 
     * @param server Instance du serveur web pour enregistrer les routes
     */
    void registerRoutes(AsyncWebServer& server) {
        // Routes GET
        server.on("/api/wifi/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
            handleGetStatus(request);
        });

        server.on("/api/wifi/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
            handleGetConfig(request);
        });

        server.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest *request) {
            handleScanNetworks(request);
        });

        // Routes POST
        AsyncCallbackJsonWebHandler* setAPConfigHandler = new AsyncCallbackJsonWebHandler(
            "/api/wifi/ap/config",
            [this](AsyncWebServerRequest* request, JsonVariant& json) {
                handleSetAPConfig(request, json.as<JsonObject>());
            }
        );
        server.addHandler(setAPConfigHandler);

        AsyncCallbackJsonWebHandler* setSTAConfigHandler = new AsyncCallbackJsonWebHandler(
            "/api/wifi/sta/config",
            [this](AsyncWebServerRequest* request, JsonVariant& json) {
                handleSetSTAConfig(request, json.as<JsonObject>());
            }
        );
        server.addHandler(setSTAConfigHandler);

        AsyncCallbackJsonWebHandler* setHostnameHandler = new AsyncCallbackJsonWebHandler(
            "/api/wifi/hostname",
            [this](AsyncWebServerRequest* request, JsonVariant& json) {
                handleSetHostname(request, json.as<JsonObject>());
            }
        );
        server.addHandler(setHostnameHandler);
    }

    /**
     * @brief Gère la requête GET pour obtenir le statut
     */
    void handleGetStatus(AsyncWebServerRequest* request) {
        StaticJsonDocument<2048> doc;
        JsonObject status = doc.to<JsonObject>();
        _wifiManager.getStatusToJson(status);
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    }

    /**
     * @brief Gère la requête GET pour obtenir la configuration
     */
    void handleGetConfig(AsyncWebServerRequest* request) {
        StaticJsonDocument<2048> doc;
        doc["hostname"] = _wifiManager.getHostname();
        JsonObject config = doc["config"].to<JsonObject>();
        _wifiManager.getConfigToJson(config);
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    }

    /**
     * @brief Gère la requête GET pour scanner les réseaux
     */
    void handleScanNetworks(AsyncWebServerRequest* request) {
        StaticJsonDocument<2048> doc;
        JsonObject networks = doc.to<JsonObject>();
        _wifiManager.getAvailableNetworks(networks);

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    }

    /**
     * @brief Gère la requête POST pour configurer le point d'accès
     */
    void handleSetAPConfig(AsyncWebServerRequest* request, const JsonObject& config) {
        bool success = _wifiManager.setAPConfigFromJson(config);
        String response = "{\"success\":" + String(success ? "true" : "false") + "}";
        request->send(200, "application/json", response);
    }

    /**
     * @brief Gère la requête POST pour configurer le client WiFi
     */
    void handleSetSTAConfig(AsyncWebServerRequest* request, const JsonObject& config) {
        bool success = _wifiManager.setSTAConfigFromJson(config);
        String response = "{\"success\":" + String(success ? "true" : "false") + "}";
        request->send(200, "application/json", response);
    }

    /**
     * @brief Gère la requête POST pour définir le nom d'hôte
     */
    void handleSetHostname(AsyncWebServerRequest* request, const JsonObject& config) {
        if (!config["hostname"].is<const char*>()) {
            request->send(400, "application/json", "{\"error\":\"Invalid hostname\"}");
            return;
        }

        bool success = _wifiManager.setHostname(config["hostname"].as<const char*>());
        String response = "{\"success\":" + String(success ? "true" : "false") + "}";
        request->send(200, "application/json", response);
    }

    /**
     * @brief Envoie les mises à jour d'état via WebSocket
     * 
     * @param force Force l'envoi même si aucun changement n'est détecté
     * @return true si des changements ont été envoyés
     */
    bool sendWsUpdates(bool force = false) {
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
};

#endif // WIFI_MANAGER_API_H 