#include <Arduino.h>
#include <WiFiManager.h>

// Définition des constantes
const char* DEFAULT_AP_SSID = "ESP32-Access-Point";
const char* DEFAULT_AP_PASSWORD = "12345678";
const char* DEFAULT_HOSTNAME = "esp32";
const char* CONFIG_FILE = "/wifi_config.json";
const IPAddress DEFAULT_AP_IP(192, 168, 4, 1);


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

        applyAPConfig();
        applySTAConfig();
    }

    /* @brief Méthodes de gestion de la configuration AP */
    /* @param const JsonObject& config : Configuration à appliquer */
    /* @return bool */
    bool WiFiManager::setAPConfig(const JsonObject& config) {
        // Créer une copie temporaire de la configuration actuelle
        ConnectionConfig tempConfig = apConfig;
        
        // Mise à jour des champs
        if (config["enabled"].is<bool>())       tempConfig.enabled = config["enabled"];
        if (config["ssid"].is<String>())        tempConfig.ssid = config["ssid"].as<String>();
        if (config["password"].is<String>())    tempConfig.password = config["password"].as<String>();
        
        // Validation et mise à jour des paramètres réseau
        if (config["ip"].is<String>()) {
            String ipStr = config["ip"].as<String>();
            if (!isValidIPv4(ipStr)) {
                return false;
            }
            tempConfig.ip.fromString(ipStr);
        }
        
        if (config["subnet"].is<String>()) {
            String subnetStr = config["subnet"].as<String>();
            if (!isValidSubnetMask(subnetStr)) {
                return false;
            }
            tempConfig.subnet.fromString(subnetStr);
        }
        
        // Mise à jour des paramètres AP spécifiques
        if (config["channel"].is<int>()) {
            int channel = config["channel"];
            if (channel < 1 || channel > 13) {
                return false;
            }
            tempConfig.channel = channel;
        }
        
        if (config["maxConnections"].is<int>()) {
            int maxConn = config["maxConnections"];
            if (maxConn < 1 || maxConn > 8) {
                return false;
            }
            tempConfig.maxConnections = maxConn;
        }
        
        if (config["hideSSID"].is<bool>()) tempConfig.hideSSID = config["hideSSID"];
        
        // La gateway est toujours l'IP de l'AP
        tempConfig.gateway = tempConfig.ip;
        
        // Si toutes les validations sont passées, on applique la nouvelle configuration
        apConfig = tempConfig;
        return applyAPConfig();
    }

    /* @brief Méthodes de gestion de la configuration STA */
    /* @param const JsonObject& config : Configuration à appliquer */
    /* @return bool */
    bool WiFiManager::setSTAConfig(const JsonObject& config) {
        // Créer une copie temporaire de la configuration actuelle
        ConnectionConfig tempConfig = staConfig;
        
        // Mise à jour des champs de base
        if (config["enabled"].is<bool>())    tempConfig.enabled = config["enabled"];
        if (config["ssid"].is<String>())     tempConfig.ssid = config["ssid"].as<String>();
        if (config["password"].is<String>()) tempConfig.password = config["password"].as<String>();

        // Gestion du DHCP et des paramètres IP
        if (config["dhcp"].is<bool>()) {
            tempConfig.dhcp = config["dhcp"];
            
            if (tempConfig.dhcp) {
                // Si DHCP activé, on efface les paramètres IP
                tempConfig.ip = IPAddress(0, 0, 0, 0);
                tempConfig.gateway = IPAddress(0, 0, 0, 0);
                tempConfig.subnet = IPAddress(0, 0, 0, 0);
            } else {
                // DHCP désactivé, on vérifie les configurations IP
                bool hasIP = config["ip"].is<String>();
                bool hasGateway = config["gateway"].is<String>();
                bool hasSubnet = config["subnet"].is<String>();
                
                // Validation de l'IP (obligatoire si DHCP off)
                if (!hasIP || !isValidIPv4(config["ip"].as<String>())) {
                    return false;
                }

                // Configuration avec IP seule
                tempConfig.ip.fromString(config["ip"].as<String>());
                
                // Si gateway ou subnet est présent, les deux doivent être présents et valides
                if (hasGateway || hasSubnet) {
                    if (!(hasGateway && hasSubnet)) {
                        return false;
                    }
                    
                    String gatewayStr = config["gateway"].as<String>();
                    String subnetStr = config["subnet"].as<String>();
                    
                    if (!isValidIPv4(gatewayStr) || !isValidSubnetMask(subnetStr)) {
                        return false;
                    }
                    
                    tempConfig.gateway.fromString(gatewayStr);
                    tempConfig.subnet.fromString(subnetStr);
                } else {
                    // Si pas de gateway/subnet spécifiés, on met des valeurs par défaut
                    tempConfig.gateway = IPAddress(0, 0, 0, 0);
                    tempConfig.subnet = IPAddress(0, 0, 0, 0);
                }
            }
        }
        
        // Si toutes les validations sont passées, on applique la nouvelle configuration
        staConfig = tempConfig;
        return applySTAConfig();
    }

    /* @brief Méthodes de scan des réseaux */
    /* @param JsonObject& obj : Objet JSON pour stocker les résultats */
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

    // Gestion du hostname
    bool WiFiManager::setHostname(const String& name) {
        hostname = name;
        return MDNS.begin(hostname.c_str());
    }

    String WiFiManager::getHostname() {
        return hostname;
    }

    // Obtenir l'état de connexion
    void WiFiManager::refreshAPStatus() {
        apStatus.connected = WiFi.softAPgetStationNum() > 0;
        apStatus.clients = WiFi.softAPgetStationNum();
        apStatus.ip = WiFi.softAPIP();
        apStatus.rssi = 0; // Non applicable pour AP
    }

    void WiFiManager::refreshSTAStatus() {
        staStatus.connected = WiFi.status() == WL_CONNECTED;
        staStatus.clients = 0; // Non applicable pour STA
        staStatus.ip = WiFi.localIP();
        staStatus.rssi = WiFi.RSSI();
    }

    // Méthodes pour connecter/déconnecter
    bool WiFiManager::connectAP() {
        apConfig.enabled = true;
        return applyAPConfig();
    }

    bool WiFiManager::disconnectAP() {
        apConfig.enabled = false;
        WiFi.softAPdisconnect(true);
        return true;
    }

    bool WiFiManager::connectSTA() {
        staConfig.enabled = true;
        return applySTAConfig();
    }

    bool WiFiManager::disconnectSTA() {
        staConfig.enabled = false;
        WiFi.disconnect(true);
        return true;
    }

    // Appliquer la configuration AP
    bool WiFiManager::applyAPConfig() {
        if (apConfig.enabled) {
            Serial.println("WIFIMANAGER: Application de la configuration AP:");
            Serial.printf("- SSID: %s\n", apConfig.ssid.c_str());
            Serial.printf("- IP: %s\n", apConfig.ip.toString().c_str());
            Serial.printf("- Canal: %d\n", apConfig.channel);
            
            WiFi.mode(staConfig.enabled ? WIFI_AP_STA : WIFI_AP);
            
            bool success = WiFi.softAP(
                apConfig.ssid.c_str(),
                apConfig.password.c_str(),
                apConfig.channel
            );
            
            if (!success) {
                Serial.println("WIFIMANAGER: Échec de la configuration du point d'accès");
                return false;
            }
            
            bool result = WiFi.softAPConfig(apConfig.ip, apConfig.gateway, apConfig.subnet);
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

    /* @brief Appliquer la configuration STA */
    /* @return bool */
    bool WiFiManager::applySTAConfig() {
        if (staConfig.enabled) {
            Serial.println("WIFIMANAGER: Application de la configuration STA:");
            Serial.printf("- SSID: %s\n", staConfig.ssid.c_str());
            Serial.printf("- Mode DHCP: %s\n", staConfig.dhcp ? "Oui" : "Non");
            
            WiFi.mode(apConfig.enabled ? WIFI_AP_STA : WIFI_STA);
            
            if (!staConfig.dhcp) {
                Serial.println("WIFIMANAGER: Configuration IP statique:");
                Serial.printf("- IP: %s\n", staConfig.ip.toString().c_str());
                Serial.printf("- Gateway: %s\n", staConfig.gateway.toString().c_str());
                Serial.printf("- Subnet: %s\n", staConfig.subnet.toString().c_str());
                WiFi.config(staConfig.ip, staConfig.gateway, staConfig.subnet);
            }
            
            WiFi.begin(staConfig.ssid.c_str(), staConfig.password.c_str());
            staStatus.enabled = true;
            staStatus.busy = true;
            staStatus.connectionStartTime = millis();
            
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

    /* @brief Sauvegarder la configuration */
    /* @return bool */
    bool WiFiManager::saveConfig() {
        Serial.println("WIFIMANAGER: Sauvegarde de la configuration...");
        
        StaticJsonDocument<1024> doc;
        
        doc["hostname"] = hostname;
        
        JsonObject apObj = doc["ap"].to<JsonObject>();
        apConfig.toJSON(apObj);
        
        JsonObject staObj = doc["sta"].to<JsonObject>();
        staConfig.toJSON(staObj);
        
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

    /* @brief Charger la configuration */
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
            setAPConfig(apJson);
            Serial.printf("- SSID: %s\n", apConfig.ssid.c_str());
            Serial.printf("- IP: %s\n", apConfig.ip.toString().c_str());
            Serial.printf("- Canal: %d\n", apConfig.channel);
        }

        // Charger config STA
        if (doc["sta"].is<JsonObject>()) {
            Serial.println("WIFIMANAGER: Configuration STA trouvée:");
            JsonObject staJson = doc["sta"].as<JsonObject>();
            setSTAConfig(staJson);
            Serial.printf("- SSID: %s\n", staConfig.ssid.c_str());
            Serial.printf("- Mode DHCP: %s\n", staConfig.dhcp ? "Oui" : "Non");
            if (!staConfig.dhcp) {
                Serial.printf("- IP Fixe: %s\n", staConfig.ip.toString().c_str());
            }
        }
        
        return true;
    }

    /* @brief Vérifier périodiquement l'état */
    /* @return void */
    void WiFiManager::poll() {
        static unsigned long lastCheck = 0;
        const unsigned long interval = 5000; // Vérifier toutes les 5 secondes
        
        unsigned long now = millis();
        if (now - lastCheck >= interval) {
            lastCheck = now;
            
            // Rafraîchir les statuts
            refreshAPStatus();
            refreshSTAStatus();
            
            // Gérer les reconnexions si nécessaire
            handleReconnections();
            
        }
    }

    /* @brief Gérer les reconnexions */
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
                } 
                else if (currentTime - staStatus.connectionStartTime >= CONNECTION_TIMEOUT) {
                    staStatus.busy = false;
                    staStatus.connected = false;
                    Serial.println("WIFIMANAGER: Timeout de connexion WiFi");
                    WiFi.disconnect(true);
                }
            }
            else if (!staStatus.connected) {
                static unsigned long lastSTARetry = 0;
                
                if (currentTime - lastSTARetry >= RETRY_INTERVAL) {
                    Serial.println("WIFIMANAGER: Nouvelle tentative de connexion WiFi...");
                    lastSTARetry = currentTime;
                    
                    WiFi.disconnect(true);
                    WiFi.begin(staConfig.ssid.c_str(), staConfig.password.c_str());
                    staStatus.busy = true;
                    staStatus.connectionStartTime = currentTime;
                }
            }
        }
        
        // Gestion du mode AP
        if (apConfig.enabled && !apStatus.enabled) {
            Serial.println("WIFIMANAGER: Redémarrage du point d'accès...");
            applyAPConfig();
        }
        
        // Gestion du mDNS
        if ((staStatus.connected || apStatus.enabled) && !MDNS.begin(hostname.c_str())) {
            MDNS.end();
            MDNS.begin(hostname.c_str());
        }
    }

    /* @brief Destructeur */
    /* @return void */
    WiFiManager::~WiFiManager() {
        // Déconnexion propre
        WiFi.disconnect(true);
        WiFi.softAPdisconnect(true);
    }


    //////////////////////
    // DIRECT ACCESS
    //////////////////////

    void WiFiManager::getAPStatus(JsonObject& obj) const {
        apStatus.toJSON(obj);
    }

    void WiFiManager::getSTAStatus(JsonObject& obj) const {
        staStatus.toJSON(obj);
    }

    void WiFiManager::getAPConfig(JsonObject& obj) const {
        apConfig.toJSON(obj);
    }

    void WiFiManager::getSTAConfig(JsonObject& obj) const {
        staConfig.toJSON(obj);
    }


    //////////////////////
    // HELPERS
    //////////////////////

    /* @brief Vérifier si une chaîne est une adresse IPv4 valide */
    /* @param const String& ip : Chaîne à vérifier */
    /* @return bool */
    bool WiFiManager::isValidIPv4(const String& ip) {
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
    bool WiFiManager::isValidSubnetMask(const String& subnet) {
        if (!isValidIPv4(subnet)) return false;
        
        // Convertir le masque en nombre 32 bits
        IPAddress mask;
        if (!mask.fromString(subnet)) return false;
        uint32_t binary = mask[0] << 24 | mask[1] << 16 | mask[2] << 8 | mask[3];
        
        // Vérifier que c'est un masque valide (tous les 1 sont contigus)
        uint32_t zeroes = ~binary + 1;
        return (binary & (zeroes - 1)) == 0;
    }

    // Méthode pour enregistrer le callback
    void WiFiManager::onStateChange(std::function<void()> callback) {
        _onStateChange = callback;
    }

    // Méthode utilitaire pour notifier les changements
    void WiFiManager::notifyStateChange() {
        if (_onStateChange) {
            _onStateChange();
        }
    }

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