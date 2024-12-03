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
    StaticJsonDocument<1024> _previousState;
    static constexpr unsigned long NOTIFICATION_INTERVAL = 500;
    static constexpr unsigned long HEARTBEAT_INTERVAL = 5000;

    /**
     * @brief Register the methods to the API server
     */
    void registerMethods() const {

        // Register API Module metadata (allows to group methods by tags in the documentation)
        const String APIMODULE_NAME = "wifi"; // API Module name (must be consistent with module name in registerMethod calls)
        _apiServer.registerModule(
            APIMODULE_NAME,                    // name
            "WiFi configuration and monitoring"     // description
        );

        // GET wifi/status
        _apiServer.registerMethod(APIMODULE_NAME, "wifi/status", 
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
                {"enabled", ParamType::Boolean},
                {"connected", ParamType::Boolean},
                {"clients", ParamType::Integer},
                {"ip", ParamType::String},
                {"rssi", ParamType::Integer}
            })
            .response("sta", {
                {"enabled", ParamType::Boolean},
                {"connected", ParamType::Boolean},
                {"ip", ParamType::String},
                {"rssi", ParamType::Integer}
            })
            .build()
        );

        // GET wifi/config
        _apiServer.registerMethod(APIMODULE_NAME, "wifi/config",
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
                {"enabled",     ParamType::Boolean},
                {"ssid",        ParamType::String},
                {"password",    ParamType::String},
                {"channel",     ParamType::Integer},
                {"ip",          ParamType::String},
                {"gateway",     ParamType::String},
                {"subnet",      ParamType::String}
            })
            .response("sta", {
                {"enabled",     ParamType::Boolean},
                {"ssid",        ParamType::String},
                {"password",    ParamType::String},
                {"dhcp",        ParamType::Boolean},
                {"ip",          ParamType::String},
                {"gateway",     ParamType::String},
                {"subnet",      ParamType::String}
            })
            .build()
        );

        // GET wifi/scan
        _apiServer.registerMethod(APIMODULE_NAME, "wifi/scan",
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
                {"ssid",        ParamType::String},
                {"rssi",        ParamType::Integer},
                {"encryption",  ParamType::Integer}
            })
            .build()
        );

        // SET wifi/ap/config
        _apiServer.registerMethod(APIMODULE_NAME, "wifi/ap/config",
            APIMethodBuilder(APIMethodType::SET, [this](const JsonObject* args, JsonObject& response) {
                bool success = _wifiManager.setAPConfigFromJson(*args);
                response["success"] = success;
                return true;
            })
            .desc("Configure Access Point")
            .param("enabled",   ParamType::Boolean)
            .param("ssid",      ParamType::String)
            .param("password",  ParamType::String)
            .param("channel",   ParamType::Integer)
            .param("ip",        ParamType::String, false)  // Optional
            .param("gateway",   ParamType::String, false)  // Optional
            .param("subnet",    ParamType::String, false)  // Optional
            .response("success",ParamType::Boolean)
            .build()
        );

        // SET wifi/sta/config
        _apiServer.registerMethod(APIMODULE_NAME, "wifi/sta/config",
            APIMethodBuilder(APIMethodType::SET, [this](const JsonObject* args, JsonObject& response) {
                bool success = _wifiManager.setSTAConfigFromJson(*args);
                response["success"] = success;
                return true;
            })
            .desc("Configure Station mode")
            .param("enabled",   ParamType::Boolean)
            .param("ssid",      ParamType::String)
            .param("password",  ParamType::String)
            .param("dhcp",      ParamType::Boolean)
            .param("ip",        ParamType::String, false)  // Optional
            .param("gateway",   ParamType::String, false)  // Optional
            .param("subnet",    ParamType::String, false)  // Optional
            .response("success",ParamType::Boolean)
            .build()
        );

        // SET wifi/hostname
        _apiServer.registerMethod(APIMODULE_NAME, "wifi/hostname",
            APIMethodBuilder(APIMethodType::SET, [this](const JsonObject* args, JsonObject& response) {
                if (!(*args)["hostname"].is<const char*>()) {
                    return false;
                }
                bool success = _wifiManager.setHostname((*args)["hostname"].as<const char*>());
                response["success"] = success;
                return true;
            })
            .desc("Set device hostname")
            .param("hostname",        ParamType::String)
            .response("success", ParamType::Boolean)
            .build()
        );

        // EVT wifi/events
        _apiServer.registerMethod(APIMODULE_NAME, "wifi/events",
            APIMethodBuilder(APIMethodType::EVT)
            .desc("WiFi status and configuration updates")
            .response("status", {
                {"ap", {
                    {"enabled",     ParamType::Boolean},
                    {"connected",   ParamType::Boolean},
                    {"clients",     ParamType::Integer},
                    {"ip",          ParamType::String},
                    {"rssi",        ParamType::Integer}
                }},
                {"sta", {
                    {"enabled",     ParamType::Boolean},
                    {"connected",   ParamType::Boolean},
                    {"ip",          ParamType::String},
                    {"rssi",        ParamType::Integer}
                }}
            })
            .response("config", {
                {"ap", {
                    {"enabled",     ParamType::Boolean},
                    {"ssid",        ParamType::String},
                    {"password",    ParamType::String},
                    {"channel",     ParamType::Integer},
                    {"ip",          ParamType::String},
                    {"gateway",     ParamType::String},
                    {"subnet",      ParamType::String}
                }},
            {"sta", {
                    {"enabled",     ParamType::Boolean},
                    {"ssid",        ParamType::String},
                    {"password",    ParamType::String},
                    {"dhcp",        ParamType::Boolean},
                    {"ip",          ParamType::String},
                    {"gateway",     ParamType::String},
                    {"subnet",      ParamType::String}
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
        StaticJsonDocument<1024> newState;
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