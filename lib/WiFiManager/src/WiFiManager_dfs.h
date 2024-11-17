#ifndef _WIFIMANAGER_DFS_H_
#define _WIFIMANAGER_DFS_H_

#include <Arduino.h>
#include <ArduinoJson.h>


//////////////////////
// CONSTANTS
//////////////////////

constexpr const char* DEFAULT_AP_SSID = "ESP32-Access-Point";
constexpr const char* DEFAULT_AP_PASSWORD = "12345678";
constexpr const char* DEFAULT_HOSTNAME = "esp32";
constexpr const char* CONFIG_FILE = "/wifi_config.json";
static const IPAddress DEFAULT_AP_IP(192, 168, 4, 1);


//////////////////////
// HELPERS
//////////////////////

/* @brief Vérifier si une chaîne est une adresse IPv4 valide */
/* @param const String& ip : Chaîne à vérifier */
/* @return bool */
inline bool isValidIPv4(const String& ip) {
    if (ip.isEmpty()) return false;
    
    int dots = 0;
    int lastDot = -1;
    
    for (int i = 0; i < ip.length(); i++) {
        if (ip[i] == '.') {
            // Vérifier la longueur du segment
            if (i - lastDot - 1 <= 0 || i - lastDot - 1 > 3) return false;
            
            // Vérifier la valeur du segment
            String segment = ip.substring(lastDot + 1, i);
            int value = segment.toInt();
            if (value < 0 || value > 255) return false;
            
            // Vérifier les zéros en tête
            if (segment.length() > 1 && segment[0] == '0') return false;
            
            dots++;
            lastDot = i;
        } else if (!isdigit(ip[i])) {
            return false;
        }
    }
    
    // Vérifier le dernier segment
    String lastSegment = ip.substring(lastDot + 1);
    if (lastSegment.length() <= 0 || lastSegment.length() > 3) return false;
    int lastValue = lastSegment.toInt();
    if (lastValue < 0 || lastValue > 255) return false;
    if (lastSegment.length() > 1 && lastSegment[0] == '0') return false;
    
    return dots == 3;
}

/* @brief Vérifier si une chaîne est un masque de sous-réseau valide */
/* @param const String& subnet : Chaîne à vérifier */
/* @return bool */
inline bool isValidSubnetMask(const String& subnet) {
    if (!isValidIPv4(subnet)) return false;
    
    // Convertir le masque en nombre 32 bits
    IPAddress mask;
    if (!mask.fromString(subnet)) return false;
    uint32_t binary = mask[0] << 24 | mask[1] << 16 | mask[2] << 8 | mask[3];
    
    // Vérifier que c'est un masque valide (tous les 1 sont contigus)
    uint32_t zeroes = ~binary + 1;
    return (binary & (zeroes - 1)) == 0;
}

//////////////////////
// STRUCTURES
//////////////////////

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
    void toJson(JsonObject& obj) const {
        obj["enabled"] = enabled;
        obj["busy"] = busy;
        obj["connected"] = connected;
        obj["ip"] = ip.toString();
        if (rssi) obj["rssi"] = rssi;
        if (clients > 0) obj["clients"] = clients;  // Uniquement si pertinent (AP)
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
    bool hideSSID = false;         // SSID caché ou visible

    // Configuration spécifique STA
    bool dhcp = true;             // true = DHCP, false = IP fixe
    IPAddress gateway = IPAddress(0, 0, 0, 0);      // Gateway
    IPAddress subnet = IPAddress(0, 0, 0, 0);       // Masque de sous-réseau

    // Méthode pour sérialiser la config en JSON
    void toJson(JsonObject& obj) const {
        obj["enabled"] = enabled;
        obj["ssid"] = ssid;
        obj["password"] = password;
        obj["ip"] = ip.toString();
        
        // Ajouter les champs spécifiques selon le type
        if (channel > 0) {  // Si c'est une config AP
            obj["gateway"] = gateway.toString();
            obj["subnet"] = subnet.toString();
            obj["channel"] = channel;
            obj["hideSSID"] = hideSSID;
        } else {  // Si c'est une config STA
            obj["dhcp"] = dhcp;
            if (!dhcp) {
                obj["gateway"] = gateway.toString();
                obj["subnet"] = subnet.toString();
            }
        }
    }

    // Méthode pour désérialiser la config depuis un JSON. Evite de répéter les vérifications pour AP et STA
    // Note : Nous ne vérifions pas si les champs obligatoires sont présents dans le JSON ou si les paramètres
    // sont valides, cela est fait dans la méthode setAPConfigFromJson et setSTAConfigFromJson (logique métier). 
    // Seuls les types des champs du JSON sont vérifiés ici.
    bool fromJson(const JsonObject& config) {
        // Créer une copie temporaire pour validation
        ConnectionConfig temp = *this;

        // Validation des types pour les paramètres hors IP & subnet
        if (config["ssid"].is<String>())     temp.ssid = config["ssid"].as<String>();
        if (config["password"].is<String>()) temp.password = config["password"].as<String>();
        if (config["channel"].is<int>())     temp.channel = config["channel"];
        if (config["dhcp"].is<bool>())       temp.dhcp = config["dhcp"];
        if (config["enabled"].is<bool>())    temp.enabled = config["enabled"];
        if (config["hideSSID"].is<bool>())   temp.hideSSID = config["hideSSID"];

        // Validation IP & subnet (doivent être valides si présents)
        if (config["ip"].is<String>()) {
            String ipStr = config["ip"].as<String>();
            if (!isValidIPv4(ipStr)) return false;
            temp.ip.fromString(ipStr);
        }
        if (config["subnet"].is<String>()) {
            String subnetStr = config["subnet"].as<String>();
            if (!isValidSubnetMask(subnetStr)) return false;
            temp.subnet.fromString(subnetStr);
        }
        
        // Si tout est valide, on applique les changements
        *this = temp;
        return true;
    }
};

#endif // _WIFIMANAGER_DFS_H_