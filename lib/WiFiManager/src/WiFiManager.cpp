#include <Arduino.h>
#include <WiFiManager.h>

    //////////////////////
    // CLASS METHODS
    //////////////////////

    /* @brief Initialisation des configurations par défaut */
    /* @return void */
    void WiFiManager::initDefaultConfig() {
        // Configuration AP par défaut
        apConfig.enabled = true;
        apConfig.ssid = DEFAULT_AP_SSID;
        apConfig.password = DEFAULT_AP_PASSWORD;
        apConfig.ip = DEFAULT_AP_IP;
        apConfig.gateway = apConfig.ip;  // Gateway = IP de l'AP
        apConfig.subnet = IPAddress(255, 255, 255, 0);
        apConfig.channel = 1;
        

        // Configuration STA par défaut
        staConfig.enabled = false;
        staConfig.dhcp = true;     // DHCP par défaut
        // Les autres champs STA restent vides jusqu'à la première connexion

        // Hostname par défaut
        hostname = DEFAULT_HOSTNAME;

        if (!applyAPConfig(apConfig)) {
            Serial.println("WIFIMANAGER: Erreur lors de l'application de la configuration AP par défaut");
        }
        if (!applySTAConfig(staConfig)) {
            Serial.println("WIFIMANAGER: Erreur lors de l'application de la configuration STA par défaut");
        }
    }



    /* @brief Initialize the WiFiManager */
    /* @return bool */
    bool WiFiManager::begin() {
        // Initialisation SPIFFS déjà faite dans main.cpp
        initDefaultConfig();

        // if (!loadConfig()) {
        //     Serial.println("WIFIMANAGER: Erreur de chargement de la configuration, utilisation des valeurs par défaut");
        // }
        
        if (!saveConfig()) {
            Serial.println("WIFIMANAGER: Erreur de sauvegarde de la configuration");
            return false;
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

    /* @brief Méthodes de gestion de la configuration AP */
    /* @param const JsonObject& config : Configuration à appliquer */
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
        if (n > 10) {  // Limite arbitraire pour la sécurité
            n = 10;
        }
        JsonDocument doc;
        JsonArray networksArray = doc["networks"].to<JsonArray>();
        for (int i = 0; i < n; ++i) {
            JsonObject networkInfo;
            networkInfo["ssid"] =       WiFi.SSID(i);
            networkInfo["rssi"] =       WiFi.RSSI(i);
            networkInfo["encryption"] = WiFi.encryptionType(i);
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
        apStatus.rssi = 0; // Non applicable pour AP
    }

    /* @brief Refresh STA status */
    /* @return void */
    void WiFiManager::refreshSTAStatus() {
        staStatus.connected = WiFi.status() == WL_CONNECTED;
        staStatus.clients = 0; // Non applicable pour STA
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
        // Validation du canal, SSID et password
        if (config.channel < 1 || config.channel > 13) return false;
        if (config.ssid.isEmpty()) return false;
        if (config.password.isEmpty()) return false;

        // Validation longueur SSID & password
        if (config.ssid.length() > 32) return false;
        if (config.password.length() < 8 || config.password.length() > 64) return false;

        // On nettoie les items de config non applicables
        config.dhcp = true;
        if (config.gateway == IPAddress(0, 0, 0, 0)) config.gateway = config.ip;
        config.subnet = IPAddress(255, 255, 255, 0);
        
        return true;
    }

    /* @brief Validate STA configuration */
    /* @param ConnectionConfig& config : Configuration to validate */
    /* @return bool */
    bool WiFiManager::validateSTAConfig(ConnectionConfig& config) {
        // Validation du SSID
        if (config.ssid.isEmpty()) return false;

        // Validation longueur SSID & password
        if (config.ssid.length() > 32) return false;
        if (config.password.length() < 8 || config.password.length() > 64) return false;

        // Gestion du DHCP et des paramètres IP STA
        if (config.dhcp) {
            // Si DHCP activé, on efface les paramètres IP
            config.ip = IPAddress(0, 0, 0, 0);
            config.gateway = IPAddress(0, 0, 0, 0);
            config.subnet = IPAddress(0, 0, 0, 0);
        } 
        else {
            // DHCP désactivé, on vérifie les configurations IP
            bool hasIP = config.ip != IPAddress(0, 0, 0, 0);
            bool hasGateway = config.gateway != IPAddress(0, 0, 0, 0);
            bool hasSubnet = config.subnet != IPAddress(0, 0, 0, 0);
            
            // Validation de l'IP (obligatoire si DHCP off)
            if (!hasIP) return false;
            
            // Si gateway ou subnet est présent, les deux doivent être présents et valides
            if (hasGateway || hasSubnet) {
                if (!(hasGateway && hasSubnet)) {
                    return false;
                }
            } else {
                // Si pas de gateway/subnet spécifiés, on met des valeurs par défaut
                config.gateway = IPAddress(0, 0, 0, 0);
                config.subnet = IPAddress(0, 0, 0, 0);
            }
        }
        // On nettoie les items de config non applicables
        config.channel = 0;
        config.hideSSID = false;
        return true;
    }

    /* @brief Apply AP configuration */
    /* @param const ConnectionConfig& config : Configuration to apply */
    /* @return bool */
    bool WiFiManager::applyAPConfig(const ConnectionConfig& config) {
        if (config.enabled) {
            Serial.println("WIFIMANAGER: Application de la configuration AP:");
            Serial.printf("- SSID: %s\n", config.ssid.c_str());
            Serial.printf("- IP: %s\n", config.ip.toString().c_str());
            Serial.printf("- Canal: %d\n", config.channel);
            
            WiFi.mode(staConfig.enabled ? WIFI_AP_STA : WIFI_AP);
            
            bool success = WiFi.softAP(
                config.ssid.c_str(),
                config.password.c_str(),
                config.channel
            );
            
            if (!success) {
                Serial.println("WIFIMANAGER: Échec de la configuration du point d'accès");
                return false;
            }

            bool result = WiFi.softAPConfig(config.ip, config.gateway, config.subnet);
            if (!result) {
                Serial.println("WIFIMANAGER: Échec de la configuration IP du point d'accès");
            } else {
                Serial.printf("WIFIMANAGER: Point d'accès configuré avec succès (IP: %s)\n", WiFi.softAPIP().toString().c_str());
            }
            
            apStatus.enabled = true;
            notifyStateChange();
            return result;
        }
        
        Serial.println("WIFIMANAGER: Désactivation du point d'accès");
        WiFi.softAPdisconnect(true);
        apStatus.enabled = false;
        notifyStateChange();
        return true;
    }

    /* @brief Apply STA configuration */
    /* @param const ConnectionConfig& config : Configuration to apply */
    /* @return bool */
    bool WiFiManager::applySTAConfig(const ConnectionConfig& config) {
        if (config.enabled) {
            Serial.println("WIFIMANAGER: Application de la configuration STA:");
            Serial.printf("- SSID: %s\n", config.ssid.c_str());
            Serial.printf("- Mode DHCP: %s\n", config.dhcp ? "Oui" : "Non");
            
            WiFi.mode(apConfig.enabled ? WIFI_AP_STA : WIFI_STA);
            
            if (!config.dhcp) {
                Serial.println("WIFIMANAGER: Configuration IP statique:");
                Serial.printf("- IP: %s\n", config.ip.toString().c_str());
                Serial.printf("- Gateway: %s\n", config.gateway.toString().c_str());
                Serial.printf("- Subnet: %s\n", config.subnet.toString().c_str());
                WiFi.config(config.ip, config.gateway, config.subnet);
            }
            
            WiFi.begin(config.ssid.c_str(), config.password.c_str());
            staStatus.enabled = true;
            staStatus.busy = true;
            
            Serial.println("WIFIMANAGER: Tentative de connexion au réseau WiFi...");
            notifyStateChange();
            return true;
        }
        
        Serial.println("WIFIMANAGER: Déconnexion du réseau WiFi");
        WiFi.disconnect(true);
        staStatus.enabled = false;
        staStatus.busy = false;
        notifyStateChange();
        return true;
    }

    /* @brief Save configuration to Flash */
    /* @return bool */
    bool WiFiManager::saveConfig() {
        Serial.println("WIFIMANAGER: Sauvegarde de la configuration...");
        
        StaticJsonDocument<1024> doc;
        
        doc["hostname"] = hostname;
        
        JsonObject apObj = doc["ap"].to<JsonObject>();
        apConfig.toJson(apObj);
        
        JsonObject staObj = doc["sta"].to<JsonObject>();
        staConfig.toJson(staObj);
        
        File file = SPIFFS.open(CONFIG_FILE, "w");
        if (!file) {
            Serial.println("WIFIMANAGER: Erreur lors de l'ouverture du fichier pour la sauvegarde");
            return false;
        }
        
        if (serializeJson(doc, file) == 0) {
            Serial.println("WIFIMANAGER: Erreur lors de l'écriture de la configuration");
            file.close();
            return false;
        }
        
        file.close();
        Serial.println("WIFIMANAGER: Configuration sauvegardée avec succès");
        return true;
    }

    /* @brief Load configuration from Flash */
    /* @return bool */
    bool WiFiManager::loadConfig() {
        Serial.println("WIFIMANAGER: Chargement de la configuration...");
        
        if (!SPIFFS.exists(CONFIG_FILE)) {
            Serial.println("WIFIMANAGER: Fichier de configuration non trouvé, utilisation des paramètres par défaut");
            return false;
        }
        
        File file = SPIFFS.open(CONFIG_FILE, "r");
        if (!file) {
            Serial.println("WIFIMANAGER: Erreur lors de l'ouverture du fichier de configuration");
            return false;
        }
        
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, file);
        file.close();
        
        if (error) {
            Serial.print("WIFIMANAGER: Erreur lors de la désérialisation JSON: ");
            Serial.println(error.c_str());
            return false;
        }

        Serial.println("WIFIMANAGER: Configuration chargée avec succès:");

        // Charger hostname
        if (doc["hostname"].is<String>()) {
            hostname = doc["hostname"].as<String>();
            Serial.printf("- Hostname: %s\n", hostname.c_str());
        }

        // Charger config AP
        if (doc["ap"].is<JsonObject>()) {
            Serial.println("WIFIMANAGER: Configuration AP trouvée:");
            JsonObject apJson = doc["ap"].as<JsonObject>();
            setAPConfigFromJson(apJson);
            Serial.printf("- SSID: %s\n", apConfig.ssid.c_str());
            Serial.printf("- IP: %s\n", apConfig.ip.toString().c_str());
            Serial.printf("- Canal: %d\n", apConfig.channel);
        }

        // Charger config STA
        if (doc["sta"].is<JsonObject>()) {
            Serial.println("WIFIMANAGER: Configuration STA trouvée:");
            JsonObject staJson = doc["sta"].as<JsonObject>();
            setSTAConfigFromJson(staJson);
            Serial.printf("- SSID: %s\n", staConfig.ssid.c_str());
            Serial.printf("- Mode DHCP: %s\n", staConfig.dhcp ? "Oui" : "Non");
            if (!staConfig.dhcp) {
                Serial.printf("- IP Fixe: %s\n", staConfig.ip.toString().c_str());
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
        
        // Gestion du mode Station
        if (staConfig.enabled) {
            if (staStatus.busy) {
                if (WiFi.status() == WL_CONNECTED) {
                    staStatus.busy = false;
                    staStatus.connected = true;
                    Serial.println("WIFIMANAGER: Connexion WiFi établie:");
                    Serial.printf("- SSID: %s\n", WiFi.SSID().c_str());
                    Serial.printf("- IP: %s\n", WiFi.localIP().toString().c_str());
                    Serial.printf("- Force du signal: %d dBm\n", WiFi.RSSI());
                    refreshSTAStatus();
                    notifyStateChange();
                } 
                else if (currentTime - lastSTAConnectionAttempt >= CONNECTION_TIMEOUT) {
                    staStatus.busy = false;
                    staStatus.connected = false;
                    Serial.println("WIFIMANAGER: Timeout de connexion WiFi");
                    WiFi.disconnect(true);
                    refreshSTAStatus();
                    notifyStateChange();
                }
            }
            else if (!staStatus.connected) {
                if (currentTime - lastSTARetry >= RETRY_INTERVAL) {
                    Serial.println("WIFIMANAGER: Nouvelle tentative de connexion WiFi...");
                    lastSTARetry = currentTime;
                    
                    WiFi.disconnect(true);
                    applySTAConfig(staConfig);
                }
            }
        }
        
        // Gestion du mode AP
        if (apConfig.enabled && !apStatus.enabled) {
            Serial.println("WIFIMANAGER: Redémarrage du point d'accès...");
            applyAPConfig(apConfig);
        }
        
        // Gestion du mDNS
        if ((staStatus.connected || apStatus.enabled) && !MDNS.begin(hostname.c_str())) {
            MDNS.end();
            MDNS.begin(hostname.c_str());
        }
    }

    /* @brief Destructor */
    /* @return void */
    WiFiManager::~WiFiManager() {
        // Déconnexion propre
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
        StaticJsonDocument<1024> doc;
        JsonObject apStatusObj = doc["ap"].to<JsonObject>();
        apStatus.toJson(apStatusObj);
        JsonObject staStatusObj = doc["sta"].to<JsonObject>();
        staStatus.toJson(staStatusObj);
        obj = doc.to<JsonObject>();
    }

    /* @brief Get configuration to JSON */
    /* @param JsonObject& obj : JSON object to store the results */
    /* @return void */
    void WiFiManager::getConfigToJson(JsonObject& obj) const {
        StaticJsonDocument<1024> doc;
        JsonObject apConfigObj = doc["ap"].to<JsonObject>();
        apConfig.toJson(apConfigObj);
        JsonObject staConfigObj = doc["sta"].to<JsonObject>();
        staConfig.toJson(staConfigObj);
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