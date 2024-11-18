#ifndef WIFI_MANAGER_API_H
#define WIFI_MANAGER_API_H

#include "WiFiManager.h"
#include "RPCServer.h"
#include <ArduinoJson.h>

class WiFiManagerAPI {
public:
    WiFiManagerAPI(WiFiManager& wifiManager, RPCServer& rpcServer) 
        : _wifiManager(wifiManager)
        , _rpcServer(rpcServer)
        , _lastNotification(0) 
    {
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
    RPCServer& _rpcServer;
    unsigned long _lastNotification;
    StaticJsonDocument<2048> _previousState;
    static constexpr unsigned long NOTIFICATION_INTERVAL = 500;

    void registerMethods() {
        // GET wifi/status
        _rpcServer.registerMethod("wifi/status", 
            RPCMethodBuilder(RPCMethodType::GET, [this](const JsonObject* args, JsonObject& response) {
                _wifiManager.getStatusToJson(response);
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
        _rpcServer.registerMethod("wifi/config",
            RPCMethodBuilder(RPCMethodType::GET, [this](const JsonObject* args, JsonObject& response) {
                _wifiManager.getConfigToJson(response);
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
        _rpcServer.registerMethod("wifi/scan",
            RPCMethodBuilder(RPCMethodType::GET, [this](const JsonObject* args, JsonObject& response) {
                _wifiManager.getAvailableNetworks(response);
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
        _rpcServer.registerMethod("wifi/ap/config",
            RPCMethodBuilder(RPCMethodType::SET, [this](const JsonObject* args, JsonObject& response) {
                bool success = _wifiManager.setAPConfigFromJson(*args);
                response["success"] = success;
                return true;
            })
            .desc("Configure Access Point")
            .param("enabled", "bool")
            .param("ssid", "string")
            .param("password", "string")
            .param("channel", "int")
            .param("ip", "string", false)
            .param("gateway", "string", false)
            .param("subnet", "string", false)
            .response("success", "bool")
            .build()
        );

        // SET wifi/sta/config
        _rpcServer.registerMethod("wifi/sta/config",
            RPCMethodBuilder(RPCMethodType::SET, [this](const JsonObject* args, JsonObject& response) {
                bool success = _wifiManager.setSTAConfigFromJson(*args);
                response["success"] = success;
                return true;
            })
            .desc("Configure Station mode")
            .param("enabled", "bool")
            .param("ssid", "string")
            .param("password", "string")
            .param("dhcp", "bool")
            .param("ip", "string", false)
            .param("gateway", "string", false)
            .param("subnet", "string", false)
            .response("success", "bool")
            .build()
        );

        // SET wifi/hostname
        _rpcServer.registerMethod("wifi/hostname",
            RPCMethodBuilder(RPCMethodType::SET, [this](const JsonObject* args, JsonObject& response) {
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
        _rpcServer.registerMethod("wifi/events",
            RPCMethodBuilder(RPCMethodType::EVT)
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

    bool sendNotification(bool force = false) {
        StaticJsonDocument<2048> newState;
        JsonObject newStatus = newState["status"].to<JsonObject>();
        JsonObject newConfig = newState["config"].to<JsonObject>();
        _wifiManager.getStatusToJson(newStatus);
        _wifiManager.getConfigToJson(newConfig);

        bool changed = (force || _previousState.isNull() || newState != _previousState);
        
        if (changed) {
            _rpcServer.broadcast("wifi/events", newState.as<JsonObject>());
            _previousState = newState;
            return true;    
        }
        return false;
    }
};

#endif // WIFI_MANAGER_API_H 