#include <Arduino.h>
#include <WiFiManager.h>

    //////////////////////
    // CLASS METHODS
    //////////////////////

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

    /* @brief Manage AP configuration */
    /* @param const ConnectionConfig& config : Configuration to apply */
    /* @return bool */
    bool WiFiManager::setAPConfig(const ConnectionConfig& config) {
        ConnectionConfig tempConfig = config;
        // Validate and clean the config then apply it
        if (!validateAPConfig(tempConfig)) return false;
        if (!applyAPConfig(tempConfig)) return false;
        apConfig = tempConfig;
        return true;
    }

    /* @brief Manage STA configuration */
    /* @param const ConnectionConfig& config : Configuration to apply */
    /* @return bool */
    bool WiFiManager::setSTAConfig(const ConnectionConfig& config) {
        // Validate and clean the config then apply it
        ConnectionConfig tempConfig = config;
        if (!validateSTAConfig(tempConfig)) return false;
        if (!applySTAConfig(tempConfig)) return false;
        staConfig = tempConfig;
        return true;
    }

    /* @brief Manage AP configuration */
    /* @param const JsonObject& config : Configuration to apply */
    /* @return bool */
    bool WiFiManager::setAPConfigFromJson(const JsonObject& config) {
        ConnectionConfig tempConfig;
        if (!tempConfig.fromJson(config)) return false;
        return setAPConfig(tempConfig);
    }

    /* @brief Manage STA configuration */
    /* @param const JsonObject& config : Configuration to apply */
    /* @return bool */
    bool WiFiManager::setSTAConfigFromJson(const JsonObject& config) {
        ConnectionConfig tempConfig;
        if (!tempConfig.fromJson(config)) return false;
        return setSTAConfig(tempConfig);
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

    /* @brief Connect AP */
    /* @return bool */
    bool WiFiManager::connectAP() {
        apConfig.enabled = true;
        return applyAPConfig(apConfig);
    }

    /* @brief Disconnect AP */
    /* @return bool */
    bool WiFiManager::disconnectAP() {
        apConfig.enabled = false;
        WiFi.softAPdisconnect(true);
        return true;
    }

    /* @brief Connect STA */
    /* @return bool */
    bool WiFiManager::connectSTA() {
        staConfig.enabled = true;
        return applySTAConfig(staConfig);
    }

    /* @brief Disconnect STA */
    /* @return bool */
    bool WiFiManager::disconnectSTA() {
        staConfig.enabled = false;
        WiFi.disconnect(true);
        return true;
    }

    /* @brief Validate AP configuration */
    /* @param ConnectionConfig& config : Configuration to validate */
    /* @return bool */
    bool WiFiManager::validateAPConfig(ConnectionConfig& config) {
        // Validation of channel, SSID and password
        if (config.channel > 13) return false;

        // Validation length of SSID & password
        if (config.ssid.length() > 32) return false;
        if ((!config.password.isEmpty() && config.password.length() < 8) || config.password.length() > 64) return false;

        // Return false if both current & submitted SSIDs, passwords and channels are empty
        if (config.ssid.isEmpty() && apConfig.ssid.isEmpty()) return false;         
        if (config.password.isEmpty() && apConfig.password.isEmpty()) return false; 
        if (config.channel == 0 && apConfig.channel == 0) return false;    

        // Add previous enabled if absent (enabledVoid true)
        if (config.enabledVoid) config.enabled = apConfig.enabled;

        // Add ssid, password & channel if absent
        if (config.ssid.isEmpty()) config.ssid = apConfig.ssid;
        if (config.password.isEmpty()) config.password = apConfig.password;
        if (config.channel == 0) config.channel = apConfig.channel;
              
        // Clean up unapplicable config items
        config.dhcp = true;
        if (config.gateway == IPAddress(0, 0, 0, 0)) config.gateway = config.ip;
        config.subnet = IPAddress(255, 255, 255, 0);
        
        return true;
    }

    /* @brief Validate STA configuration */
    /* @param ConnectionConfig& config : Configuration to validate */
    /* @return bool */
    bool WiFiManager::validateSTAConfig(ConnectionConfig& config) {

        // Validation length of SSID & password
        if (config.ssid.length() > 32) return false;
        if ((!config.password.isEmpty() &&config.password.length() < 8) || config.password.length() > 64) return false;

        // Add previous enabled if absent (enabledVoid true)
        if (config.enabledVoid) config.enabled = staConfig.enabled;

        // Manage DHCP and IP settings for STA
        if (config.dhcp) {
            // If DHCP enabled, clear IP settings
            config.ip = IPAddress(0, 0, 0, 0);
            config.gateway = IPAddress(0, 0, 0, 0);
            config.subnet = IPAddress(0, 0, 0, 0);
        } 
        else { // DHCP disabled, check IP configurations

            // Special cases for robustness :
            // - Current IP, gateway & subnet are >0 : we assume the user wants to change all IP settings
            // - Current IP is >0, gateway & subnet are BOTH 0 : we assume the user wants to change the fixed IP only
            // - Current IP is 0, gateway & subnet are BOTH >0 : we assume the user wants to change gateway & subnet
            // - Current IP, gateway & subnet are 0 : we assume the user wants to use the previous values
            // - Corner case: if the user has set gateway & subnet and wants to set them to default, they will have to toggle DHCP

            // Recover current IP, gateway & subnet if absent
            if (config.ip == IPAddress(0, 0, 0, 0)) config.ip = staConfig.ip;
            if (config.gateway == IPAddress(0, 0, 0, 0) && config.subnet == IPAddress(0, 0, 0, 0)) {
                config.gateway = staConfig.gateway;
                config.subnet = staConfig.subnet;
            }
            
            // Check if IP, gateway & subnet are not null
            bool hasIP = config.ip != IPAddress(0, 0, 0, 0);
            bool hasGateway = config.gateway != IPAddress(0, 0, 0, 0);
            bool hasSubnet = config.subnet != IPAddress(0, 0, 0, 0);
            
            // Validation of IP (required if DHCP off)
            if (!hasIP) return false;
            
            // If gateway or subnet is present, both must be present and valid
            if (hasGateway || hasSubnet) {
                if (!(hasGateway && hasSubnet)) {
                    return false;
                }
            } else {
                // If no gateway/subnet specified, set default values
                config.gateway = IPAddress(0, 0, 0, 0);
                config.subnet = IPAddress(0, 0, 0, 0);
            }
        }

        // Add previous ssid and password if both are empty
        // If only SSID is filled, keep password blank (possibly a new unencrypted network)
        // If only password is filled, add current SSID (possibly trying a new password)
        if (config.ssid.isEmpty() && config.password.isEmpty()) {   
            config.ssid = staConfig.ssid;
            config.password = staConfig.password;
        }
        else if (config.ssid.isEmpty()) {
            config.ssid = staConfig.ssid;
        }
        else if (config.password.isEmpty()) {
            config.password = staConfig.password;
        }

        // Clean up unapplicable config items
        config.channel = 0;
        config.hideSSID = false;
        return true;
    }

    /* @brief Apply AP configuration */
    /* @param const ConnectionConfig& config : Configuration to apply */
    /* @return bool */
    bool WiFiManager::applyAPConfig(const ConnectionConfig& config) {
        // If it's a configuration change, disconnect the AP first
        if (config.enabled && apConfig.enabled) {
            WiFi.softAPdisconnect(true);
            apStatus.enabled = false;
        }

        if (config.enabled) {
            Serial.println("wifi_ap: Applying AP configuration:");
            Serial.printf("- SSID: %s\n", config.ssid.c_str());
            Serial.printf("- IP: %s\n", config.ip.toString().c_str());
            Serial.printf("- Channel: %d\n", config.channel);
            
            WiFi.mode(staConfig.enabled ? WIFI_AP_STA : WIFI_AP);
            
            bool success = WiFi.softAP(
                config.ssid.c_str(),
                config.password.c_str(),
                config.channel
            );
            
            if (!success) {
                Serial.println("wifi_ap: Failed to configure access point, aborting...");
                WiFi.softAPdisconnect(true);
                apStatus.enabled = false;
                notifyStateChange();
                return false;
            } else {
                bool result = WiFi.softAPConfig(config.ip, config.gateway, config.subnet);
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
    /* @param const ConnectionConfig& config : Configuration to apply */
    /* @return bool */
    bool WiFiManager::applySTAConfig(const ConnectionConfig& config) {
        // If it's a configuration change, disconnect the STA first
        if (config.enabled && staConfig.enabled) {
            WiFi.disconnect(true);
            staStatus.enabled = false;
        }

        if (config.enabled) {
            Serial.println("wifi_sta: Applying STA configuration:");
            Serial.printf("- SSID: %s\n", config.ssid.c_str());
            Serial.printf("- DHCP mode: %s\n", config.dhcp ? "Yes" : "No");
            
            WiFi.mode(apConfig.enabled ? WIFI_AP_STA : WIFI_STA);
            
            if (!config.dhcp) {
                Serial.println("wifi_sta: Static IP configuration:");
                Serial.printf("- IP: %s\n", config.ip.toString().c_str());
                Serial.printf("- Gateway: %s\n", config.gateway.toString().c_str());
                Serial.printf("- Subnet: %s\n", config.subnet.toString().c_str());
                WiFi.config(config.ip, config.gateway, config.subnet);
            }
            
            WiFi.begin(config.ssid.c_str(), config.password.c_str());
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
            setAPConfigFromJson(apJson);
            Serial.printf("- SSID: %s\n", apConfig.ssid.c_str());
            Serial.printf("- IP: %s\n", apConfig.ip.toString().c_str());
            Serial.printf("- Channel: %d\n", apConfig.channel);
        }

        // Charger config STA
        if (doc["sta"].is<JsonObject>()) {
            Serial.println("wifi_config: STA configuration found:");
            JsonObject staJson = doc["sta"].as<JsonObject>();
            setSTAConfigFromJson(staJson);
            Serial.printf("- SSID: %s\n", staConfig.ssid.c_str());
            Serial.printf("- DHCP mode: %s\n", staConfig.dhcp ? "Yes" : "No");
            if (!staConfig.dhcp) {
                Serial.printf("- Fixed IP: %s\n", staConfig.ip.toString().c_str());
            }
        }
        
        return true;
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
/*********************************** Getters **********************************/
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