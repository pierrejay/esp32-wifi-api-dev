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
    // WiFi status and configuration
    String hostname;
    ConnectionConfig apConfig, staConfig;
    ConnectionStatus apStatus, staStatus;
    StaticJsonDocument<256> _lastStatus;

    // WiFi connection parameters
    static constexpr unsigned long POLL_INTERVAL = 2000;        // 2 seconds between status updates
    static constexpr unsigned long CONNECTION_TIMEOUT = 30000;  // 30 seconds timeout for connection attempts
    static constexpr unsigned long RETRY_INTERVAL = 30000;      // 30 seconds between retries after disconnection
    unsigned long lastConnectionCheck = 0;
    unsigned long lastSTARetry = 0;
    unsigned long lastSTAConnectionAttempt = 0;

    // Lifecycle methods
    void initDefaultConfig();
    void handleReconnections();

    // Config validation and application
    bool validateAPConfig(ConnectionConfig& config);
    bool validateSTAConfig(ConnectionConfig& config);
    bool applyAPConfig(const ConnectionConfig& config);
    bool applySTAConfig(const ConnectionConfig& config);
    
    // Config file management
    bool saveConfig();
    bool loadConfig();

    // Helpers
    bool isValidIPv4(const String& ip);
    bool isValidSubnetMask(const String& subnet);

    // Send push notifications to the API server
    void notifyStateChange();
    std::function<void()> _onStateChange;

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

    // Method to register a state change callback (API server notifications)
    void onStateChange(std::function<void()> callback);
};

#endif // _WIFIMANAGER_H_
