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

        //@API_DOC_SECTION_START
        // API Module name (must be consistent between module info & registerMethod calls)
        const String APIMODULE_NAME = "wifi"; 
       
       // Register API Module metadata (allows to group methods by tags in the documentation)
        _apiServer.registerModuleInfo(
            APIMODULE_NAME,                                 // Name
            "WiFi configuration and monitoring",     // Description
            "1.0.0"                                      // Version
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
                {"enabled", APIParamType::Boolean},
                {"connected", APIParamType::Boolean},
                {"clients", APIParamType::Integer},
                {"ip", APIParamType::String},
                {"rssi", APIParamType::Integer}
            })
            .response("sta", {
                {"enabled", APIParamType::Boolean},
                {"connected", APIParamType::Boolean},
                {"ip", APIParamType::String},
                {"rssi", APIParamType::Integer}
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
                {"enabled",     APIParamType::Boolean},
                {"ssid",        APIParamType::String},
                {"password",    APIParamType::String},
                {"channel",     APIParamType::Integer},
                {"ip",          APIParamType::String},
                {"gateway",     APIParamType::String},
                {"subnet",      APIParamType::String}
            })
            .response("sta", {
                {"enabled",     APIParamType::Boolean},
                {"ssid",        APIParamType::String},
                {"password",    APIParamType::String},
                {"dhcp",        APIParamType::Boolean},
                {"ip",          APIParamType::String},
                {"gateway",     APIParamType::String},
                {"subnet",      APIParamType::String}
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
                {"ssid",        APIParamType::String},
                {"rssi",        APIParamType::Integer},
                {"encryption",  APIParamType::Integer}
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
            .param("enabled",   APIParamType::Boolean)
            .param("ssid",      APIParamType::String)
            .param("password",  APIParamType::String)
            .param("channel",   APIParamType::Integer)
            .param("ip",        APIParamType::String, false)  // Optional
            .param("gateway",   APIParamType::String, false)  // Optional
            .param("subnet",    APIParamType::String, false)  // Optional
            .response("success",APIParamType::Boolean)
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
            .param("enabled",   APIParamType::Boolean)
            .param("ssid",      APIParamType::String)
            .param("password",  APIParamType::String)
            .param("dhcp",      APIParamType::Boolean)
            .param("ip",        APIParamType::String, false)  // Optional
            .param("gateway",   APIParamType::String, false)  // Optional
            .param("subnet",    APIParamType::String, false)  // Optional
            .response("success",APIParamType::Boolean)
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
            .param("hostname",        APIParamType::String)
            .response("success", APIParamType::Boolean)
            .build()
        );

        // EVT wifi/events
        _apiServer.registerMethod(APIMODULE_NAME, "wifi/events",
            APIMethodBuilder(APIMethodType::EVT)
            .desc("WiFi status and configuration updates")
            .response("status", {
                {"ap", {
                    {"enabled",     APIParamType::Boolean},
                    {"connected",   APIParamType::Boolean},
                    {"clients",     APIParamType::Integer},
                    {"ip",          APIParamType::String},
                    {"rssi",        APIParamType::Integer}
                }},
                {"sta", {
                    {"enabled",     APIParamType::Boolean},
                    {"connected",   APIParamType::Boolean},
                    {"ip",          APIParamType::String},
                    {"rssi",        APIParamType::Integer}
                }}
            })
            .response("config", {
                {"ap", {
                    {"enabled",     APIParamType::Boolean},
                    {"ssid",        APIParamType::String},
                    {"password",    APIParamType::String},
                    {"channel",     APIParamType::Integer},
                    {"ip",          APIParamType::String},
                    {"gateway",     APIParamType::String},
                    {"subnet",      APIParamType::String}
                }},
            {"sta", {
                    {"enabled",     APIParamType::Boolean},
                    {"ssid",        APIParamType::String},
                    {"password",    APIParamType::String},
                    {"dhcp",        APIParamType::Boolean},
                    {"ip",          APIParamType::String},
                    {"gateway",     APIParamType::String},
                    {"subnet",      APIParamType::String}
                }}
            })
            .build()
        );
        //@API_DOC_SECTION_END
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