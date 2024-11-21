#ifndef WIFIMANAGERAPI_H
#define WIFIMANAGERAPI_H

#include "WiFiManager.h"
#include "APIServer.h"
#include <ArduinoJson.h>

class WiFiManagerAPI {
public:
    WiFiManagerAPI(WiFiManager& wifiManager, APIServer& apiServer) 
        : _wifiManager(wifiManager)
        , _apiServer(apiServer)
        , _lastNotification(0)
        , _lastHeartbeat(0)
    {
        // Subscribe to WiFi state changes
        _wifiManager.onStateChange([this]() {
            sendNotification(true);
        });
        
        registerMethods();
    }

    /**
     * @brief Checks for state changes and sends updates via WebSocket
     * 
     * Must be called regularly in the main loop.
     * Sends updates via WebSocket when state changes are detected
     * and the minimum interval has elapsed.
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
    unsigned long _lastHeartbeat;
    StaticJsonDocument<2048> _previousState;
    static constexpr unsigned long NOTIFICATION_INTERVAL = 500;
    static constexpr unsigned long HEARTBEAT_INTERVAL = 5000;

    /**
     * @brief Register the methods to the API server
     */
    void registerMethods() {
        // GET wifi/status
        _apiServer.registerMethod("wifi/status", 
            APIMethodBuilder(APIMethodType::GET, [this](const JsonObject* args, JsonObject& response) {
                Serial.println("WIFIAPI: Exécution de GET wifi/status");
                _wifiManager.getStatusToJson(response);
                
                String debug;
                serializeJson(response, debug);
                Serial.printf("WIFIAPI: Réponse status: %s\n", debug.c_str());
                
                return true;
            })
            .desc("Get WiFi status")
            .response("ap", {
                {"enabled", "bool"},
                {"connected", "bool"},
                {"clients", "int"},
                {"ip", "string"},
                {"rssi", "int"}
            })
            .response("sta", {
                {"enabled", "bool"},
                {"connected", "bool"},
                {"ip", "string"},
                {"rssi", "int"}
            })
            .build()
        );

        // GET wifi/config
        _apiServer.registerMethod("wifi/config",
            APIMethodBuilder(APIMethodType::GET, [this](const JsonObject* args, JsonObject& response) {
                Serial.println("WIFIAPI: Exécution de GET wifi/config");
                _wifiManager.getConfigToJson(response);
                
                String debug;
                serializeJson(response, debug);
                Serial.printf("WIFIAPI: Réponse config: %s\n", debug.c_str());
                
                return true;
            })
            .desc("Get WiFi configuration")
            .response("ap", {
                {"enabled", "bool"},
                {"ssid", "string"},
                {"password", "string"},
                {"channel", "int"},
                {"ip", "string"},
                {"gateway", "string"},
                {"subnet", "string"}
            })
            .response("sta", {
                {"enabled", "bool"},
                {"ssid", "string"},
                {"password", "string"},
                {"dhcp", "bool"},
                {"ip", "string"},
                {"gateway", "string"},
                {"subnet", "string"}
            })
            .build()
        );

        // GET wifi/scan
        _apiServer.registerMethod("wifi/scan",
            APIMethodBuilder(APIMethodType::GET, [this](const JsonObject* args, JsonObject& response) {
                Serial.println("WIFIAPI: Exécution de GET wifi/scan");
                _wifiManager.getAvailableNetworks(response);
                
                String debug;
                serializeJson(response, debug);
                Serial.printf("WIFIAPI: Réponse scan: %s\n", debug.c_str());
                
                return true;
            })
            .desc("Scan available WiFi networks")
            .response("networks", {
                {"ssid", "string"},
                {"rssi", "int"},
                {"encryption", "int"}
            })
            .build()
        );

        // SET wifi/ap/config
        _apiServer.registerMethod("wifi/ap/config",
            APIMethodBuilder(APIMethodType::SET, [this](const JsonObject* args, JsonObject& response) {
                bool success = _wifiManager.setAPConfigFromJson(*args);
                response["success"] = success;
                return true;
            })
            .desc("Configure Access Point")
            .param("enabled", "bool")
            .param("ssid", "string")
            .param("password", "string")
            .param("channel", "int")
            .param("ip", "string", false)       // Optional
            .param("gateway", "string", false)  // Optional
            .param("subnet", "string", false)   // Optional
            .response("success", "bool")
            .build()
        );

        // SET wifi/sta/config
        _apiServer.registerMethod("wifi/sta/config",
            APIMethodBuilder(APIMethodType::SET, [this](const JsonObject* args, JsonObject& response) {
                bool success = _wifiManager.setSTAConfigFromJson(*args);
                response["success"] = success;
                return true;
            })
            .desc("Configure Station mode")
            .param("enabled", "bool")
            .param("ssid", "string")
            .param("password", "string")
            .param("dhcp", "bool")
            .param("ip", "string", false)       // Optional
            .param("gateway", "string", false)  // Optional
            .param("subnet", "string", false)   // Optional
            .response("success", "bool")
            .build()
        );

        // SET wifi/hostname
        _apiServer.registerMethod("wifi/hostname",
            APIMethodBuilder(APIMethodType::SET, [this](const JsonObject* args, JsonObject& response) {
                if (!(*args)["hostname"].is<const char*>()) {
                    return false;
                }
                bool success = _wifiManager.setHostname((*args)["hostname"].as<const char*>());
                response["success"] = success;
                return true;
            })
            .desc("Set device hostname")
            .param("hostname", "string")
            .response("success", "bool")
            .build()
        );

        // EVT wifi/events
        _apiServer.registerMethod("wifi/events",
            APIMethodBuilder(APIMethodType::EVT)
            .desc("WiFi status and configuration updates")
            .response("status", {
                {"ap", {
                    {"enabled", "bool"},
                    {"connected", "bool"},
                    {"clients", "int"},
                    {"ip", "string"},
                    {"rssi", "int"}
                }},
                {"sta", {
                    {"enabled", "bool"},
                    {"connected", "bool"},
                    {"ip", "string"},
                    {"rssi", "int"}
                }}
            })
            .response("config", {
                {"ap", {
                    {"enabled", "bool"},
                    {"ssid", "string"},
                    {"password", "string"},
                    {"channel", "int"},
                    {"ip", "string"},
                    {"gateway", "string"},
                    {"subnet", "string"}
                }},
                {"sta", {
                    {"enabled", "bool"},
                    {"ssid", "string"},
                    {"password", "string"},
                    {"dhcp", "bool"},
                    {"ip", "string"},
                    {"gateway", "string"},
                    {"subnet", "string"}
                }}
            })
            .build()
        );
    }

    /**
     * @brief Send a notification to the API server
     * @param force Force the notification even if the state has not changed
     * @return True if the notification has been sent, false otherwise
     */
    bool sendNotification(bool force = false) {
        unsigned long now = millis();
        StaticJsonDocument<2048> newState;
        JsonObject newStatus = newState["status"].to<JsonObject>();
        JsonObject newConfig = newState["config"].to<JsonObject>();
        _wifiManager.getStatusToJson(newStatus);
        _wifiManager.getConfigToJson(newConfig);

        bool changed = (force || _previousState.isNull() || newState != _previousState);
        bool heartbeatNeeded = (now - _lastHeartbeat >= HEARTBEAT_INTERVAL);
        
        if (changed || heartbeatNeeded) {
            _apiServer.broadcast("wifi/events", newState.as<JsonObject>());
            _previousState = newState;
            _lastHeartbeat = now;
            
            if (heartbeatNeeded && !changed) {
                Serial.println("WIFIAPI: Envoi du heartbeat");
            }
            return true;    
        }
        return false;
    }
};

#endif // WIFIMANAGERAPI_H 