#ifndef _WIFIMANAGER_DFS_H_
#define _WIFIMANAGER_DFS_H_

#include <Arduino.h>
#include <ArduinoJson.h>


/******************************************************************************/
/************************** Constants Definitions *****************************/
/******************************************************************************/

constexpr const char* DEFAULT_AP_SSID = "ESP32-Access-Point";
constexpr const char* DEFAULT_AP_PASSWORD = "12345678";
constexpr const char* DEFAULT_HOSTNAME = "esp32";
constexpr const char* CONFIG_FILE = "/wifi_config.json";
static const IPAddress DEFAULT_AP_IP(192, 168, 4, 1);



/******************************************************************************/
/**************************** Helper Functions ********************************/
/******************************************************************************/

/* @brief Check if a string is a valid IPv4 address */
/* @param const String& ip : String to check */
/* @return bool */
inline bool isValidIPv4(const String& ip) {
    if (ip.isEmpty()) return false;
    
    int dots = 0;
    int lastDot = -1;
    
    for (int i = 0; i < ip.length(); i++) {
        if (ip[i] == '.') {
            // Check segment length
            if (i - lastDot - 1 <= 0 || i - lastDot - 1 > 3) return false;
            
            // Check segment value
            String segment = ip.substring(lastDot + 1, i);
            int value = segment.toInt();
            if (value < 0 || value > 255) return false;
            
            // Check for leading zeros
            if (segment.length() > 1 && segment[0] == '0') return false;
            
            dots++;
            lastDot = i;
        } else if (!isdigit(ip[i])) {
            return false;
        }
    }
    
    // Check last segment
    String lastSegment = ip.substring(lastDot + 1);
    if (lastSegment.length() <= 0 || lastSegment.length() > 3) return false;
    int lastValue = lastSegment.toInt();
    if (lastValue < 0 || lastValue > 255) return false;
    if (lastSegment.length() > 1 && lastSegment[0] == '0') return false;
    
    return dots == 3;
}

/* @brief Check if a string is a valid subnet mask */
/* @param const String& subnet : String to check */
/* @return bool */
inline bool isValidSubnetMask(const String& subnet) {
    if (!isValidIPv4(subnet)) return false;
    
    // Convert subnet to 32-bit number
    IPAddress mask;
    if (!mask.fromString(subnet)) return false;
    uint32_t binary = mask[0] << 24 | mask[1] << 16 | mask[2] << 8 | mask[3];
    
    // Check that it's a valid mask (all 1s are contiguous)
    uint32_t zeroes = ~binary + 1;
    return (binary & (zeroes - 1)) == 0;
}


/******************************************************************************/
/****************************** Data Structures *******************************/
/******************************************************************************/

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
    // General configuration
    bool enabled = false;
    String ssid = "";
    String password = "";
    IPAddress ip = IPAddress(0, 0, 0, 0);           // Fixed IP for AP

    // Specific AP configuration
    int channel = 0;           // WiFi channel (1-13)
    bool hideSSID = false;     // Hidden or visible SSID

    // Specific STA configuration
    bool dhcp = true;             // true = DHCP, false = IP fixe
    IPAddress gateway = IPAddress(0, 0, 0, 0);      // Gateway
    IPAddress subnet = IPAddress(0, 0, 0, 0);       // Subnet mask

    // Method to serialize config to JSON
    void toJson(JsonObject& obj) const {
        obj["enabled"] = enabled;
        obj["ssid"] = ssid;
        obj["password"] = password;
        obj["ip"] = ip.toString();
        
        // Add specific fields according to the type
        if (channel > 0) {  // If it's an AP config
            obj["gateway"] = gateway.toString();
            obj["subnet"] = subnet.toString();
            obj["channel"] = channel;
            obj["hideSSID"] = hideSSID;
        } else {  // If it's a STA config
            obj["dhcp"] = dhcp;
            if (!dhcp) {
                obj["gateway"] = gateway.toString();
                obj["subnet"] = subnet.toString();
            }
        }
    }

    // Method to deserialize config from JSON. Avoids repeating validations for AP and STA
    // Note: We don't check if mandatory fields are present in JSON or if parameters
    // are valid, this is done in setAPConfigFromJson and setSTAConfigFromJson methods 
    // (business logic). Only JSON field types are validated here.
    bool fromJson(const JsonObject& config) {
        // Create a temporary copy for validation
        ConnectionConfig temp = *this;

        // Validation of types for parameters other than IP & subnet
        if (config["ssid"].is<String>())     temp.ssid = config["ssid"].as<String>();
        if (config["password"].is<String>()) temp.password = config["password"].as<String>();
        if (config["channel"].is<int>())     temp.channel = config["channel"];
        if (config["dhcp"].is<bool>())       temp.dhcp = config["dhcp"];
        if (config["enabled"].is<bool>())    temp.enabled = config["enabled"];
        if (config["hideSSID"].is<bool>())   temp.hideSSID = config["hideSSID"];

        // Validation IP & subnet (must be valid if present)
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
        
        // If everything is valid, apply the changes
        *this = temp;
        return true;
    }
};

#endif // _WIFIMANAGER_DFS_H_