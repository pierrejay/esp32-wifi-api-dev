#ifndef WIFI_MANAGER_API_H
#define WIFI_MANAGER_API_H

#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <APIServer.h>
#include <ArduinoJson.h>
#include "WiFiManager.h"

#define WS_SEND_INTERVAL 500 // Intervalle minimum entre les mises à jour WebSocket en millisecondes

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
            sendWsUpdates();
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
            if (detectChanges()) {
                sendWsUpdates();
                _lastWsUpdate = now;
            }
        }
    }

private:
    WiFiManager& _wifiManager;
    APIServer& _apiServer;
    unsigned long _lastWsUpdate;
    StaticJsonDocument<1024> _previousState;

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
        StaticJsonDocument<1024> doc;
        JsonObject status = doc.to<JsonObject>();
        
        // Les statuts sont déjà rafraîchis par la boucle principale
        JsonObject apStatus = status["ap"].to<JsonObject>();
        _wifiManager.getAPStatus(apStatus);
        
        JsonObject staStatus = status["sta"].to<JsonObject>();
        _wifiManager.getSTAStatus(staStatus);

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    }

    /**
     * @brief Gère la requête GET pour obtenir la configuration
     */
    void handleGetConfig(AsyncWebServerRequest* request) {
        StaticJsonDocument<1024> doc;
        
        doc["hostname"] = _wifiManager.getHostname();
        
        JsonObject apConfig = doc["ap"].to<JsonObject>();
        _wifiManager.getAPConfig(apConfig);
        
        JsonObject staConfig = doc["sta"].to<JsonObject>();
        _wifiManager.getSTAConfig(staConfig);

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
     * @brief Détecte les changements d'état
     * @return true si des changements sont détectés
     */
    bool detectChanges() {
        StaticJsonDocument<1024> currentState;
        
        JsonObject apStatus = currentState["ap"].to<JsonObject>();
        _wifiManager.getAPStatus(apStatus);
        
        JsonObject staStatus = currentState["sta"].to<JsonObject>();
        _wifiManager.getSTAStatus(staStatus);

        bool changed = (_previousState.isNull() || currentState != _previousState);
        if (changed) {
            _previousState = currentState;
        }
        return changed;
    }

    /**
     * @brief Envoie les mises à jour d'état via WebSocket
     */
    void sendWsUpdates() {
        StaticJsonDocument<1024> doc;
        doc["type"] = "wifi_status";
        
        JsonObject data = doc["data"].to<JsonObject>();
        JsonObject apStatus = data["ap"].to<JsonObject>();
        _wifiManager.getAPStatus(apStatus);
        
        JsonObject staStatus = data["sta"].to<JsonObject>();
        _wifiManager.getSTAStatus(staStatus);

        String message;
        serializeJson(doc, message);
        _apiServer.push(message);
    }
};

#endif // WIFI_MANAGER_API_H 