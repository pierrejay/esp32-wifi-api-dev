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
#include <optional>

#define FORCE_WIFI_DEFAULT_CONFIG // Uncomment to force the use of the default configuration (useful if the configuration file is corrupted)

static constexpr const char* DEFAULT_AP_SSID = "ESP32-Access-Point";
static constexpr const char* DEFAULT_AP_PASSWORD = "12345678";
static constexpr const char* DEFAULT_HOSTNAME = "esp32";
static constexpr const char* CONFIG_FILE = "/wifi_config.json";
static const IPAddress DEFAULT_AP_IP = IPAddress(192, 168, 4, 1);

class WiFiManager {

public:

// Common structure for connection status
    struct ConnectionStatus {
        bool enabled = false;
        bool connected = false;
        bool busy = false;

        IPAddress ip = IPAddress(0, 0, 0, 0);
        int clients = 0;     // Used only for AP
        int rssi = 0;       // Used only for STA
        
        // Method to serialize status to JSON
        void toJson(JsonObject& obj) const {
            obj["enabled"] = enabled;
            obj["busy"] = busy;
            obj["connected"] = connected;
            obj["ip"] = ip.toString();
            if (rssi) obj["rssi"] = rssi;
            if (clients > 0) obj["clients"] = clients;  // Only if relevant (AP)
        }
    };

    // Structure for the connection configuration
    struct ConnectionConfig {
        // Common fields
        std::optional<bool> enabled;
        std::optional<String> ssid;
        std::optional<String> password;
        std::optional<IPAddress> ip;
        // AP only
        std::optional<int> channel;
        std::optional<bool> hideSSID;
        // STA only
        std::optional<bool> dhcp;
        std::optional<IPAddress> gateway;
        std::optional<IPAddress> subnet;

        // Parse JSON configuration - returns false if any field has wrong type
        bool fromJson(const JsonObject& config) {
            // If a field exists, it MUST be of the correct type
            if (config.containsKey("enabled")) {
                if (!config["enabled"].is<bool>()) return false;
                enabled = config["enabled"].as<bool>();
            }
            
            if (config.containsKey("ssid")) {
                if (!config["ssid"].is<String>()) return false;
                ssid = config["ssid"].as<String>();
            }
            
            if (config.containsKey("password")) {
                if (!config["password"].is<String>()) return false;
                password = config["password"].as<String>();
            }
            
            if (config.containsKey("channel")) {
                if (!config["channel"].is<int>()) return false;
                channel = config["channel"].as<int>();
            }
            
            if (config.containsKey("dhcp")) {
                if (!config["dhcp"].is<bool>()) return false;
                dhcp = config["dhcp"].as<bool>();
            }
            
            if (config.containsKey("hideSSID")) {
                if (!config["hideSSID"].is<bool>()) return false;
                hideSSID = config["hideSSID"].as<bool>();
            }

            if (config.containsKey("ip")) {
                if (!config["ip"].is<String>()) return false;
                String ipStr = config["ip"].as<String>();
                if (!isValidIPv4(ipStr)) return false;
                IPAddress ipAddr;
                if (!ipAddr.fromString(ipStr)) return false;
                ip = ipAddr;
            }

            if (config.containsKey("gateway")) {
                if (!config["gateway"].is<String>()) return false;
                String gwStr = config["gateway"].as<String>();
                if (!isValidIPv4(gwStr)) return false;
                IPAddress gwAddr;
                if (!gwAddr.fromString(gwStr)) return false;
                gateway = gwAddr;
            }

            if (config.containsKey("subnet")) {
                if (!config["subnet"].is<String>()) return false;
                String subnetStr = config["subnet"].as<String>();
                if (!isValidSubnetMask(subnetStr)) return false;
                IPAddress subnetAddr;
                if (!subnetAddr.fromString(subnetStr)) return false;
                subnet = subnetAddr;
            }

            return true;
        }

        // Serialize configuration to JSON
        void toJson(JsonObject& obj) const {
            // Champs communs - on peut utiliser value_or() pour simplifier
            if (enabled.has_value()) obj["enabled"] = *enabled;
            if (ssid.has_value()) obj["ssid"] = *ssid;
            if (password.has_value()) obj["password"] = *password;
            if (ip.has_value()) obj["ip"] = ip->toString();
            if (gateway.has_value()) obj["gateway"] = gateway->toString();
            if (subnet.has_value()) obj["subnet"] = subnet->toString();
            // AP specific fields
            if (channel.has_value()) obj["channel"] = *channel;
            if (hideSSID.has_value()) obj["hideSSID"] = *hideSSID;
            // STA specific fields
            if (dhcp.has_value()) obj["dhcp"] = *dhcp;
        }
    
    };

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

    // WiFi authentication types
    static constexpr const char* AUTH_MODE_STRINGS[] = {
        "OPEN",          // WIFI_AUTH_OPEN
        "WEP",           // WIFI_AUTH_WEP
        "WPA_PSK",       // WIFI_AUTH_WPA_PSK
        "WPA2_PSK",      // WIFI_AUTH_WPA2_PSK
        "WPA_WPA2_PSK",  // WIFI_AUTH_WPA_WPA2_PSK
        "ENTERPRISE",    // WIFI_AUTH_ENTERPRISE
        "WPA3_PSK",      // WIFI_AUTH_WPA3_PSK
        "WPA2_WPA3_PSK", // WIFI_AUTH_WPA2_WPA3_PSK
        "WAPI_PSK",      // WIFI_AUTH_WAPI_PSK
        "OWE",           // WIFI_AUTH_OWE
        "WPA3_ENT_192",  // WIFI_AUTH_WPA3_ENT_192
        "MAX",           // WIFI_AUTH_MAX   
        "UNKNOWN"        // WIFI_AUTH_UNKNOWN (security type not supported)
    };
    
    // Lifecycle methods
    bool initDefaultConfig();
    void handleReconnections();

    // Config validation and application
    // The config argument is not const because it will be modified if needed
    // (e.g. wrong values, missing)
    bool validateAPConfig(ConnectionConfig& config);
    bool validateSTAConfig(ConnectionConfig& config);
    bool applyAPConfig(const ConnectionConfig& config);
    bool applySTAConfig(const ConnectionConfig& config);
    
    // Config file management
    bool saveConfig();
    bool loadConfig();

    // Helpers
    static bool isValidIPv4(const String& ip);
    static bool isValidSubnetMask(const String& subnet);

    // Send push notifications to the API server
    void notifyStateChange();
    std::function<void()> _onStateChange;


};

#endif // _WIFIMANAGER_H_
