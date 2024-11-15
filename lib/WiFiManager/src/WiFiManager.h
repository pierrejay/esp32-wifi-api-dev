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

// Déclarations externes des constantes
extern const char* DEFAULT_AP_SSID;
extern const char* DEFAULT_AP_PASSWORD;
extern const char* DEFAULT_HOSTNAME;
extern const char* CONFIG_FILE;
extern const IPAddress DEFAULT_AP_IP;

// Structure commune pour le statut des connexions
struct ConnectionStatus {
    bool enabled = false;
    bool connected = false;
    bool busy = false;

    IPAddress ip = IPAddress(0, 0, 0, 0);
    int clients = 0;     // Utilisé uniquement pour AP
    int rssi = 0;       // Utilisé uniquement pour STA
    unsigned long connectionStartTime = 0;  // Pour suivre le temps de connexion
    
    // Méthode pour sérialiser le statut en JSON
    void toJSON(JsonObject& obj) const {
        obj["enabled"] = enabled;
        obj["busy"] = busy;
        obj["connected"] = connected;
        obj["ip"] = ip.toString();
        obj["sta_rssi"] = rssi;
        if (clients > 0) obj["ap_clients"] = clients;  // Uniquement si pertinent
    }
};

// Structure pour la configuration de la connexion
struct ConnectionConfig {
    // Configuration générale
    bool enabled = false;
    String ssid = "";
    String password = "";
    IPAddress ip = IPAddress(0, 0, 0, 0);           // IP fixe pour l'AP

    // Configuration spécifique AP
    int channel = 0;           // Canal WiFi (1-13)
    int maxConnections = 0;    // Nombre max de clients
    bool hideSSID = false;         // SSID caché ou visible
    // Configuration spécifique STA
    bool dhcp = true;             // true = DHCP, false = IP fixe
    IPAddress gateway = IPAddress(0, 0, 0, 0);      // Toujours égal à l'IP de l'AP
    IPAddress subnet = IPAddress(0, 0, 0, 0);       // Masque de sous-réseau

    // Méthode pour sérialiser la config en JSON
    void toJSON(JsonObject& obj) const {
        obj["enabled"] = enabled;
        obj["ssid"] = ssid;
        obj["password"] = password;
        obj["ip"] = ip.toString();
        
        // Ajouter les champs spécifiques selon le type
        if (channel > 0) {  // Si c'est une config AP
            obj["ap_gateway"] = gateway.toString();
            obj["ap_subnet"] = subnet.toString();
            obj["ap_channel"] = channel;
            obj["ap_maxConnections"] = maxConnections;
            obj["ap_hideSSID"] = hideSSID;
        } else {  // Si c'est une config STA
            obj["dhcp"] = dhcp;
            if (!dhcp) {
                obj["sta_gateway"] = gateway.toString();
                obj["sta_subnet"] = subnet.toString();
            }
        }
    }
};


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
    bool applyAPConfig();
    bool applySTAConfig();
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
    bool setAPConfig(const JsonObject& config);
    bool setSTAConfig(const JsonObject& config);
    void getAvailableNetworks(JsonObject& obj);

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

    // Direct object access methods
    void getAPStatus(JsonObject& obj) const;
    void getSTAStatus(JsonObject& obj) const;
    void getAPConfig(JsonObject& obj) const;
    void getSTAConfig(JsonObject& obj) const;

    // State change notification callback registration
    void onStateChange(std::function<void()> callback);
};

#endif // _WIFIMANAGER_H_
