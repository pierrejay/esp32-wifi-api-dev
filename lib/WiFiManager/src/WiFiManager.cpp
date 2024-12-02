#include <Arduino.h>
#include <WiFiManager.h>


/******************************************************************************/
/************************** Lifecycle management ******************************/
/******************************************************************************/

/* @brief Initialize default configurations */
/* @return void */
bool WiFiManager::initDefaultConfig() {
    // Default AP configuration
    apConfig.enabled = true;
    apConfig.ssid = DEFAULT_AP_SSID;
    apConfig.password = DEFAULT_AP_PASSWORD;
    apConfig.ip = DEFAULT_AP_IP;
    apConfig.gateway = apConfig.ip;  // Gateway = AP IP
    apConfig.subnet = IPAddress(255, 255, 255, 0);
    apConfig.channel = 1;
    

    // Default STA configuration
    staConfig.enabled = false;
    staConfig.dhcp = true;     // DHCP by default
    // Other STA fields remain empty until first connection

    // Default hostname
    hostname = DEFAULT_HOSTNAME;

    if (!applyAPConfig(apConfig)) {
        Serial.println("WIFIMANAGER: Error applying default AP configuration");
        return false;
    }
    if (!applySTAConfig(staConfig)) {
        Serial.println("WIFIMANAGER: Error applying default STA configuration");
        return false;
    }
    return true;
}


/* @brief Initialize the WiFiManager */
/* @return bool */
bool WiFiManager::begin() {
    // SPIFFS initialization already done in main.cpp
    #ifdef FORCE_WIFI_DEFAULT_CONFIG
        initDefaultConfig();
    #else
        if (!loadConfig()) {
            Serial.println("WIFIMANAGER: Error loading configuration, using default values");
            return initDefaultConfig();
        }
    #endif
    
    if (!saveConfig()) {
        Serial.println("WIFIMANAGER: Error saving configuration");
    }

    return true;
}

/* @brief Refresh AP status */
/* @return void */
void WiFiManager::refreshAPStatus() {
    apStatus.connected = WiFi.softAPgetStationNum() > 0;
    apStatus.clients = WiFi.softAPgetStationNum();
    apStatus.ip = WiFi.softAPIP();
    apStatus.rssi = 0; // Not applicable for AP
}

/* @brief Refresh STA status */
/* @return void */
void WiFiManager::refreshSTAStatus() {
    staStatus.connected = WiFi.status() == WL_CONNECTED;
    staStatus.clients = 0; // Not applicable for STA
    staStatus.ip = WiFi.localIP();
    staStatus.rssi = WiFi.RSSI();
}

/* @brief Periodically check the state */
/* @return void */
void WiFiManager::poll() {
    
    unsigned long now = millis();
    if (now - lastConnectionCheck >= POLL_INTERVAL) {
        lastConnectionCheck = now;
        
        // Refresh status
        refreshAPStatus();
        refreshSTAStatus();
        
        // Handle reconnections if necessary
        handleReconnections();
        
    }
}

/* @brief Handle reconnections */
/* @return void */
void WiFiManager::handleReconnections() {
    unsigned long currentTime = millis();
    
    // Manage STA mode
    if (staConfig.enabled) {
        if (staStatus.busy) {
            if (WiFi.status() == WL_CONNECTED) {
                staStatus.busy = false;
                staStatus.connected = true;
                Serial.println("wifi_sta: WiFi connection established:");
                Serial.printf("- SSID: %s\n", WiFi.SSID().c_str());
                Serial.printf("- IP: %s\n", WiFi.localIP().toString().c_str());
                Serial.printf("- Signal strength: %d dBm\n", WiFi.RSSI());
                refreshSTAStatus();
                notifyStateChange();
            } 
            else if (currentTime - lastSTAConnectionAttempt >= CONNECTION_TIMEOUT) {
                staStatus.busy = false;
                staStatus.connected = false;
                Serial.println("wifi_sta: WiFi connection timeout");
                WiFi.disconnect(true);
                refreshSTAStatus();
                notifyStateChange();
            }
        }
        else if (!staStatus.connected) {
            if (currentTime - lastSTARetry >= RETRY_INTERVAL) {
                Serial.println("wifi_sta: New WiFi connection attempt...");
                lastSTARetry = currentTime;
                
                WiFi.disconnect(true);
                applySTAConfig(staConfig);
            }
        }
    }
    
    // Manage AP mode
    if (apConfig.enabled && !apStatus.enabled) {
        Serial.println("wifi_ap: Restarting the access point...");
        applyAPConfig(apConfig);
    }
    
    // Manage mDNS
    if ((staStatus.connected || apStatus.enabled) && !MDNS.begin(hostname.c_str())) {
        MDNS.end();
        MDNS.begin(hostname.c_str());
    }
}

/* @brief Destructor */
/* @return void */
WiFiManager::~WiFiManager() {
    // Clean disconnection
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
}


/******************************************************************************/
/*************** Configuration validation & application ***********************/
/******************************************************************************/
//
// Memo : optionals use pointer dereferencing (*) to get value
//        optionals use direct access (=) to set value
//        std::nullopt for empty value
//        has_value() & value_or() for safe reading of optionals
//

/* @brief Validate AP configuration */
/* @param ConnectionConfig& config : Configuration to validate */
/* @return bool true if valid, false otherwise */
bool WiFiManager::validateAPConfig(ConnectionConfig& config) {
    // Channel: if present check value, else use AP value, or default (1)
    if (config.channel.has_value() && config.channel > 13) return false;
    else if (!config.channel.has_value()) config.channel = apConfig.channel.value_or(1);

    // SSID: if present check length, else use AP value, or fail
    if (config.ssid.has_value() && config.ssid->length() > 32) return false;
    else if (!config.ssid.has_value() && apConfig.ssid.has_value()) config.ssid = *apConfig.ssid;
    else if (!config.ssid.has_value() && !apConfig.ssid.has_value()) return false;

    // Password: if present check length, else use AP value, or default ("" = no password))
    if (config.password.has_value() && (config.password->length() < 8 || config.password->length() > 64)) return false;
    else if (!config.password.has_value() && apConfig.password.has_value()) config.password = *apConfig.password;
    else if (!config.password.has_value() && !apConfig.password.has_value()) return false;

    // Enabled: if absent use AP value, or set to false
    if (!config.enabled.has_value()) config.enabled = apConfig.enabled.has_value() ? *apConfig.enabled : false;

    // HideSSID: if absent use AP value, or set to false
    if (!config.hideSSID.has_value()) config.hideSSID = apConfig.hideSSID.has_value() ? *apConfig.hideSSID : false;
        
    // Clean up unapplicable config items
    config.dhcp = std::nullopt;
    if (config.gateway.has_value() && (config.gateway == IPAddress(0, 0, 0, 0))) config.gateway = *apConfig.ip;
    else if (!config.gateway.has_value()) config.gateway = *apConfig.ip;
    config.subnet = IPAddress(255, 255, 255, 0);
    
    return true;
}

/* @brief Validate STA configuration */
/* @param ConnectionConfig& config : Configuration to validate */
/* @return bool true if valid, false otherwise */
bool WiFiManager::validateSTAConfig(ConnectionConfig& config) {
    // SSID: if present check length, else use STA value, or fail
    if (config.ssid && config.ssid->length() > 32) return false;
    else if (!config.ssid.has_value() && staConfig.ssid.has_value()) config.ssid = *staConfig.ssid;
    else if (!config.ssid.has_value() && !staConfig.ssid.has_value()) return false;

    // Password: if present check length, else use STA value (no password = empty string)
    if (config.password.has_value() && (config.password->length() > 64)) return false;
    else if (!config.password.has_value()) config.password = *staConfig.password; // Copy even if nullopt

    // Enabled: if absent use STA value, or set to false
    if (!config.enabled.has_value()) config.enabled = staConfig.enabled.has_value() ? *staConfig.enabled : false;

    // DHCP: if absent use STA value, or default (true)
    // If true, clear IP config
    // if false, check IP config
    if (!config.dhcp.has_value()) config.dhcp = staConfig.dhcp.has_value() ? *staConfig.dhcp : true;
    // If dhcp=true, clear IP config
    if (*config.dhcp) {
        config.ip = std::nullopt;
        config.gateway = std::nullopt;
        config.subnet = std::nullopt;
    } 
    else { // If dhcp=false, IP+gateway+subnet should be present and valid
        
        // If present and 0.0.0.0, fail
        if (config.ip.has_value() && config.ip == IPAddress(0,0,0,0)) return false;
        if (config.gateway.has_value() && config.gateway == IPAddress(0,0,0,0)) return false;
        if (config.subnet.has_value() && config.subnet == IPAddress(0,0,0,0)) return false;

        // If absent, use STA value, and if STA is absent or 0.0.0.0, fail
        if (!config.ip.has_value()) {
            if (staConfig.ip.has_value() && staConfig.ip != IPAddress(0,0,0,0)) config.ip = *staConfig.ip;
            else return false;
        }
        if (!config.gateway.has_value()) {
            if (staConfig.gateway.has_value() && staConfig.gateway != IPAddress(0,0,0,0)) config.gateway = *staConfig.gateway;
            else return false;
        }
        if (!config.subnet.has_value()) {
            if (staConfig.subnet.has_value() && staConfig.subnet != IPAddress(0,0,0,0)) config.subnet = *staConfig.subnet;
            else return false;
        }
    }

    // Clean up AP fields
    config.channel = std::nullopt;
    config.hideSSID = std::nullopt;

    return true;
}

/* @brief Apply AP configuration */
/* @param const ConnectionConfig& config : Configuration to apply (should be validated beforehand) */
/* @return bool true on success, false on failure */
bool WiFiManager::applyAPConfig(const ConnectionConfig& config) {
    // Fail on missing values
    if (!config.enabled.has_value()) return false;
    if (!config.ssid.has_value()) return false;
    if (!config.password.has_value()) return false;
    if (!config.ip.has_value()) return false;
    if (!config.channel.has_value()) return false;
    if (!config.gateway.has_value()) return false;
    if (!config.subnet.has_value()) return false;

    // If change of config, disconnect AP first
    if (*config.enabled) {
        WiFi.softAPdisconnect(true);
        apStatus.enabled = false;
    }

    if (*config.enabled) {
        Serial.println("wifi_ap: Applying AP configuration:");
        Serial.printf("- SSID: %s\n", config.ssid->c_str());
        Serial.printf("- IP: %s\n", config.ip->toString().c_str());
        Serial.printf("- Channel: %d\n", *config.channel);
        
        WiFi.mode(staStatus.enabled ? WIFI_AP_STA : WIFI_AP);
        
        bool success = WiFi.softAP(
            config.ssid->c_str(),
            config.password->c_str(),
            *config.channel
        );
        
        if (!success) {
            Serial.println("wifi_ap: Failed to configure access point, aborting...");
            WiFi.softAPdisconnect(true);
            apStatus.enabled = false;
            notifyStateChange();
            return false;
        } else {
            bool result = WiFi.softAPConfig(*config.ip, *config.gateway, *config.subnet);
            if (!result) {
                Serial.println("wifi_ap: Failed to configure AP IP, aborting...");
                WiFi.softAPdisconnect(true);
                apStatus.enabled = false;
                notifyStateChange();
                return false;
            } else {
                Serial.printf("wifi_ap: Access point configured successfully (IP: %s)\n", WiFi.softAPIP().toString().c_str());
                apStatus.enabled = true;
                notifyStateChange();
                return true;
            }
        }
    }
    
    // Normal case : disable the AP (always successful)
    Serial.println("wifi_ap: Disabling the access point");
    WiFi.softAPdisconnect(true);
    apStatus.enabled = false;
    notifyStateChange();
    return true;
}

/* @brief Apply STA configuration */
/* @param const ConnectionConfig& config : Configuration to apply (should be validated beforehand) */
/* @return bool true on success, false on failure */
bool WiFiManager::applySTAConfig(const ConnectionConfig& config) {
    // Fail on missing values
    if (!config.enabled.has_value()) return false;
    if (!config.ssid.has_value()) return false;
    if (!config.password.has_value()) return false;
    if (!config.dhcp.has_value()) return false;

    // Fail on missing/invalid values for dhcp=false
    if (!*config.dhcp) {
        if (!config.ip.has_value() || !config.gateway.has_value() || !config.subnet.has_value()) return false;
        if (*config.ip == IPAddress(0,0,0,0) || *config.gateway == IPAddress(0,0,0,0) || *config.subnet == IPAddress(0,0,0,0)) return false;
    }

    // If it's a configuration change, disconnect the STA first
    if (*config.enabled && staConfig.enabled) {
        WiFi.disconnect(true);
        staStatus.enabled = false;
    }

    if (*config.enabled) {
        Serial.println("wifi_sta: Applying STA configuration:");
        Serial.printf("- SSID: %s\n", config.ssid->c_str());
        Serial.printf("- DHCP mode: %s\n", *config.dhcp ? "Yes" : "No");
        
        WiFi.mode(apConfig.enabled ? WIFI_AP_STA : WIFI_STA);
        
        if (!*config.dhcp) {
            Serial.println("wifi_sta: Static IP configuration:");
            Serial.printf("- IP: %s\n", config.ip->toString().c_str());
            Serial.printf("- Gateway: %s\n", config.gateway->toString().c_str());
            Serial.printf("- Subnet: %s\n", config.subnet->toString().c_str());
            WiFi.config(*config.ip, *config.gateway, *config.subnet);
        }
        
        WiFi.begin(config.ssid->c_str(), config.password->c_str());
        staStatus.enabled = true;
        staStatus.busy = true;
        
        Serial.println("wifi_sta: Attempting to connect to WiFi...");
        notifyStateChange();
        return true;
    }
    
    Serial.println("wifi_sta: Disconnecting from WiFi");
    WiFi.disconnect(true);
    staStatus.enabled = false;
    staStatus.busy = false;
    notifyStateChange();
    return true;
}

/* @brief Set AP configuration */
/* @param const ConnectionConfig& config : Configuration to apply */
/* @return bool true on success, false on failure */
bool WiFiManager::setAPConfig(const ConnectionConfig& config) {
    ConnectionConfig tempConfig = config;
    if (!validateAPConfig(tempConfig)) {
        Serial.println("wifi_ap: Invalid AP configuration, aborting...");
        return false;
    }
    if (!applyAPConfig(tempConfig)) {
        Serial.println("wifi_ap: Failed to apply AP configuration, aborting...");
        return false;
    }
    apConfig = tempConfig; // Apply the validated config
    return true;
}

/* @brief Set STA configuration */
/* @param const ConnectionConfig& config : Configuration to apply */
/* @return bool true on success, false on failure */
bool WiFiManager::setSTAConfig(const ConnectionConfig& config) {
    ConnectionConfig tempConfig = config;
    if (!validateSTAConfig(tempConfig)) {
        Serial.println("wifi_sta: Invalid STA configuration, aborting...");
        return false;
    }
    if (!applySTAConfig(tempConfig)) {
        Serial.println("wifi_sta: Failed to apply STA configuration, aborting...");
        return false;
    }
    staConfig = tempConfig; // Apply the validated config
    return true;
}


/******************************************************************************/
/************************** API functions *************************************/
/******************************************************************************/

/* @brief Get status to JSON */
/* @param JsonObject& obj : JSON object to store the results */
/* @return void */
void WiFiManager::getStatusToJson(JsonObject& obj) const {
    JsonObject apStatusObj = obj["ap"].to<JsonObject>();
    apStatus.toJson(apStatusObj);
    JsonObject staStatusObj = obj["sta"].to<JsonObject>();
    staStatus.toJson(staStatusObj);
}

/* @brief Get configuration to JSON */
/* @param JsonObject& obj : JSON object to store the results */
/* @return void */
void WiFiManager::getConfigToJson(JsonObject& obj) const {
    JsonObject apConfigObj = obj["ap"].to<JsonObject>();
    apConfig.toJson(apConfigObj);
    JsonObject staConfigObj = obj["sta"].to<JsonObject>();
    staConfig.toJson(staConfigObj);
}

/* @brief Set AP configuration from JSON */
/* @param const JsonObject& config : Configuration to apply */
/* @return bool */
bool WiFiManager::setAPConfigFromJson(const JsonObject& config) {
    ConnectionConfig tempConfig;
    if (!tempConfig.fromJson(config)) {
        Serial.println("wifi_ap: Invalid JSON configuration");
        return false;
    }
    return setAPConfig(tempConfig);
}

/* @brief Set STA configuration from JSON */
/* @param const JsonObject& config : Configuration to apply */
/* @return bool */
bool WiFiManager::setSTAConfigFromJson(const JsonObject& config) {
    ConnectionConfig tempConfig;
    if (!tempConfig.fromJson(config)) {
        Serial.println("wifi_sta: Invalid JSON configuration");
        return false;
    }
    return setSTAConfig(tempConfig);
}

/* @brief Set the hostname */
/* @param const String& name : Hostname to set */
/* @return bool */
bool WiFiManager::setHostname(const String& name) {
    hostname = name;
    return MDNS.begin(hostname.c_str());
}

/* @brief Get the hostname */
/* @return String */
String WiFiManager::getHostname() {
    return hostname;
}

/* @brief Scan available networks */
/* @param JsonObject& obj : JSON object to store the results */
/* @return void */
void WiFiManager::getAvailableNetworks(JsonObject& obj) {
    int n = WiFi.scanNetworks();
    if (n > 10) {  // Arbitrary limit for safety
        n = 10;
    }
    StaticJsonDocument<1024> doc;
    JsonArray networksArray = doc["networks"].to<JsonArray>();
    for (int i = 0; i < n; ++i) {
        uint8_t encType = static_cast<uint8_t>(WiFi.encryptionType(i));
        encType = encType < 12 ? encType : 12;
        JsonObject networkInfo;
        networkInfo["ssid"] =       WiFi.SSID(i);
        networkInfo["rssi"] =       WiFi.RSSI(i);
        networkInfo["encryption"] = AUTH_MODE_STRINGS[encType];
        networksArray.add(networkInfo);
    }
    obj = doc.to<JsonObject>();
}


/* @brief Register the callback */
/* @param std::function<void()> callback : Callback function */
/* @return void */
void WiFiManager::onStateChange(std::function<void()> callback) {
    _onStateChange = callback;
}

/* @brief Notify state changes */
/* @return void */
void WiFiManager::notifyStateChange() {
    if (_onStateChange) {
        _onStateChange();
    }
}

/* @brief Save configuration to Flash */
/* @return bool */
bool WiFiManager::saveConfig() {
    Serial.println("wifi_config: Saving configuration...");
    
    StaticJsonDocument<1024> doc;
    
    doc["hostname"] = hostname;
    
    JsonObject apObj = doc["ap"].to<JsonObject>();
    apConfig.toJson(apObj);
    
    JsonObject staObj = doc["sta"].to<JsonObject>();
    staConfig.toJson(staObj);
    
    File file = SPIFFS.open(CONFIG_FILE, "w");
    if (!file) {
        Serial.println("wifi_config: Error opening file for saving");
        return false;
    }
    
    if (serializeJson(doc, file) == 0) {
        Serial.println("wifi_config: Error saving configuration");
        file.close();
        return false;
    }
    
    file.close();
    Serial.println("wifi_config: Configuration saved successfully");
    return true;
}

/* @brief Load configuration from Flash */
/* @return bool */
bool WiFiManager::loadConfig() {
    bool success = false;
    Serial.println("wifi_config: Loading configuration...");
    
    if (!SPIFFS.exists(CONFIG_FILE)) {
        Serial.println("wifi_config: Configuration file not found, using default parameters");
        return false;
    }
    
    File file = SPIFFS.open(CONFIG_FILE, "r");
    if (!file) {
        Serial.println("wifi_config: Error opening configuration file");
        return false;
    }
    
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Serial.print("wifi_config: Error during JSON deserialization: ");
        Serial.println(error.c_str());
        return false;
    }

    Serial.println("wifi_config: Configuration loaded successfully:");

    // Load hostname
    if (doc["hostname"].is<String>()) {
        hostname = doc["hostname"].as<String>();
        Serial.printf("- Hostname: %s\n", hostname.c_str());
    }

    // Charger config AP
    if (doc["ap"].is<JsonObject>()) {
        Serial.println("wifi_config: AP configuration found:");
        JsonObject apJson = doc["ap"].as<JsonObject>();
        ConnectionConfig tempConfig;
        if (tempConfig.fromJson(apJson) && applyAPConfig(tempConfig)) {
            Serial.printf("- SSID: %s\n", apConfig.ssid->c_str());
            Serial.printf("- IP: %s\n", apConfig.ip->toString().c_str());
            Serial.printf("- Channel: %d\n", *apConfig.channel);
        }
        else {
            Serial.println("wifi_config: Error applying AP configuration, using default values");
            success = false;
        }
    }

    // Charger config STA
    if (doc["sta"].is<JsonObject>()) {
        Serial.println("wifi_config: STA configuration found:");
        JsonObject staJson = doc["sta"].as<JsonObject>();
        ConnectionConfig tempConfig;
        if (tempConfig.fromJson(staJson) && applySTAConfig(tempConfig)) {
            Serial.printf("- SSID: %s\n", staConfig.ssid->c_str());
            Serial.printf("- DHCP mode: %s\n", *staConfig.dhcp ? "Yes" : "No");
            if (!staConfig.dhcp) {
                Serial.printf("- Fixed IP: %s\n", staConfig.ip->toString().c_str());
            }
            if (success) success = true;
        }
        else {
            Serial.println("wifi_config: Error applying STA configuration, using default values");
            success = false;
        }
    }
    
    return success;
}



/******************************************************************************/
/*********************************** Helpers **********************************/
/******************************************************************************/

/* @brief Check if a string is a valid IPv4 address */
/* @param const String& ip : String to check */
/* @return bool true if valid, false otherwise */
bool WiFiManager::isValidIPv4(const String& ip) {
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
/* @return bool true if valid, false otherwise */
bool WiFiManager::isValidSubnetMask(const String& subnet) {
    if (!isValidIPv4(subnet)) return false;
    
    // Convert subnet to 32-bit number
    IPAddress mask;
    if (!mask.fromString(subnet)) return false;
    uint32_t binary = mask[0] << 24 | mask[1] << 16 | mask[2] << 8 | mask[3];
    
    // Check that it's a valid mask (all 1s are contiguous)
    uint32_t zeroes = ~binary + 1;
    return (binary & (zeroes - 1)) == 0;
}
