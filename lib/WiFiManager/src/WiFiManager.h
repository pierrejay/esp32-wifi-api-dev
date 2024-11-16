#ifndef _WIFIMANAGER_H_
#define _WIFIMANAGER_H_

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <vector>
#include <functional>
#include "WiFiManager_dfs.h"

class WiFiManager {
private:
    String hostname;
    ConnectionConfig apConfig, staConfig;
    ConnectionStatus apStatus, staStatus;
    StaticJsonDocument<256> _lastStatus;
    std::function<void()> _onStateChange;

    static const unsigned long CONNECTION_TIMEOUT = 30000;  // 30 secondes timeout
    static const unsigned long RETRY_INTERVAL = 30000;      // 30 secondes entre les tentatives

    void initDefaultConfig();

    bool validateAPConfig(ConnectionConfig& config);
    bool validateSTAConfig(ConnectionConfig& config);
    bool applyAPConfig(const ConnectionConfig& config);
    bool applySTAConfig(const ConnectionConfig& config);
    
    bool saveConfig();
    bool loadConfig();
    bool isValidIPv4(const String& ip);
    bool isValidSubnetMask(const String& subnet);
    void handleReconnections();
    void notifyStateChange();

public:
    WiFiManager() = default;
    bool begin();
    ~WiFiManager();

    // Configuration methods
    bool setAPConfig(const ConnectionConfig& config);
    bool setSTAConfig(const ConnectionConfig& config);
    bool setAPConfigFromJson(const JsonObject& config);
    bool setSTAConfigFromJson(const JsonObject& config);
    void getAvailableNetworks(JsonObject& obj);

    // Getters for status and configuration
    void getStatusToJson(JsonObject& obj) const;
    void getConfigToJson(JsonObject& obj) const;

    // Hostname management
    bool setHostname(const String& name);
    String getHostname();

    // Status methods
    void refreshAPStatus();
    void refreshSTAStatus();

    // Connection control
    bool connectAP();
    bool disconnectAP();
    bool connectSTA();
    bool disconnectSTA();

    // Main loop method
    void poll();

    // State change notification callback registration
    void onStateChange(std::function<void()> callback);
};

#endif // _WIFIMANAGER_H_
