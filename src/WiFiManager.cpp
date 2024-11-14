#include <Arduino.h>
#include <WiFiManager.h>


    WiFiManager::WiFiManager(APIServer& apiServer) : _apiServer(&apiServer) {
        if (!SPIFFS.begin(true)) {
            Serial.println("SPIFFS Mount Failed");
        }
        initDefaultConfig();
        loadConfig();
        registerAPIRoutes();
        saveConfig();
    }

    // Initialisation des configurations par défaut
    void WiFiManager::initDefaultConfig() {
        // Configuration AP par défaut
        apConfig.enabled = true;
        apConfig.ssid = DEFAULT_AP_SSID;
        apConfig.password = DEFAULT_AP_PASSWORD;
        apConfig.ip = IPAddress(192, 168, 4, 1);
        apConfig.gateway = apConfig.ip;  // Gateway = IP de l'AP
        apConfig.subnet = IPAddress(255, 255, 255, 0);
        apConfig.channel = 1;

        // Configuration STA par défaut
        staConfig.enabled = false;
        staConfig.dhcp = true;     // DHCP par défaut
        // Les autres champs STA restent vides jusqu'à la première connexion

        // Hostname par défaut
        hostname = DEFAULT_HOSTNAME;
    }

    // Fonction pour envoyer l'état via WebSocket
    void WiFiManager::wsBroadcastStatus() {
        // Construire le statut actuel
        refreshAPStatus();
        refreshSTAStatus();
        
        StaticJsonDocument<256> currentStatus;
        
        JsonObject apObj = currentStatus["ap"].to<JsonObject>();
        apStatus.toJSON(apObj);

        JsonObject staObj = currentStatus["sta"].to<JsonObject>();
        staStatus.toJSON(staObj);

        // Comparer avec l'état précédent
        bool stateChanged = (_lastStatus.isNull() || _lastStatus != currentStatus);

        // N'envoyer que si l'état a changé
        if (stateChanged) {
            StaticJsonDocument<512> wrapper;
            wrapper["type"] = "wifi_status";
            wrapper["data"] = currentStatus;
            
            String message;
            serializeJson(wrapper, message);
            _apiServer->textAll(message);

            _lastStatus = currentStatus;
        }
    }

    // Méthodes de gestion de la configuration AP
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

    // Méthodes de gestion de la configuration STA
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

    // Méthodes de scan des réseaux
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
            WiFi.mode(staConfig.enabled ? WIFI_AP_STA : WIFI_AP);
            
            bool success = WiFi.softAP(
                apConfig.ssid.c_str(),
                apConfig.password.c_str(),
                apConfig.channel,
                apConfig.hideSSID,
                apConfig.maxConnections
            );
            
            if (!success) return false;
            
            bool result = WiFi.softAPConfig(apConfig.ip, apConfig.gateway, apConfig.subnet);
            apStatus.enabled = true; // Vrai car AP activé même si échec de la configuration
            wsBroadcastStatus(); // Envoyer l'état après le changement
            return result;
        }
        WiFi.softAPdisconnect(true);
        apStatus.enabled = false;
        wsBroadcastStatus(); // Envoyer l'état après le changement
        return true;
    }

    // Appliquer la configuration STA
    bool WiFiManager::applySTAConfig() {
        if (staConfig.enabled) {
            WiFi.mode(apConfig.enabled ? WIFI_AP_STA : WIFI_STA);
            
            if (!staConfig.dhcp) {
                WiFi.config(staConfig.ip, staConfig.gateway, staConfig.subnet);
            }
            
            WiFi.begin(staConfig.ssid.c_str(), staConfig.password.c_str());
            staStatus.enabled = true;
            staStatus.busy = true;
            staStatus.connectionStartTime = millis();  // Démarrer le timer
            
            return true;
        }
        
        WiFi.disconnect(true);
        staStatus.enabled = false;
        staStatus.busy = false;
        wsBroadcastStatus();
        return true;
    }

    // Sauvegarder la configuration
    bool WiFiManager::saveConfig() {
        StaticJsonDocument<1024> doc;
        
        // Ajouter une gestion des erreurs
        if (!SPIFFS.begin()) {
            Serial.println("Erreur SPIFFS lors de la sauvegarde");
            return false;
        }
        
        doc["hostname"] = hostname;
        
        JsonObject apObj = doc["ap"].to<JsonObject>();
        apConfig.toJSON(apObj);
        
        JsonObject staObj = doc["sta"].to<JsonObject>();
        staConfig.toJSON(staObj);
        
        File file = SPIFFS.open(CONFIG_FILE, "w");
        if (!file) return false;
        
        serializeJson(doc, file);
        file.close();
        return true;
    }

    // Charger la configuration
    bool WiFiManager::loadConfig() {
        if (!SPIFFS.exists(CONFIG_FILE)) return false;
        
        File file = SPIFFS.open(CONFIG_FILE, "r");
        if (!file) return false;
        
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, file);
        file.close();
        
        if (error) return false;

        // Charger hostname
        if (doc["hostname"].is<String>()) hostname = doc["hostname"].as<String>();

        // Charger config AP
        if (doc["ap"].is<JsonObject>()) {
            setAPConfig(doc["ap"].as<JsonObject>());
        }

        // Charger config STA
        if (doc["sta"].is<JsonObject>()) {
            setSTAConfig(doc["sta"].as<JsonObject>());
        }
        
        return true;
    }

    void WiFiManager::registerAPIRoutes() {
        _apiServer->addAPI([this](AsyncWebServer& server) {
            // Route pour obtenir la configuration complète
            server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
                StaticJsonDocument<1024> doc;
                doc["hostname"] = this->hostname;
                
                JsonObject apObj = doc["ap"].to<JsonObject>();
                apConfig.toJSON(apObj);
                
                JsonObject staObj = doc["sta"].to<JsonObject>();
                staConfig.toJSON(staObj);
                
                String response;
                serializeJson(doc, response);
                request->send(200, "application/json", response);
            });

            // Route pour mettre à jour le hostname
            server.on("/api/hostname", HTTP_POST, [](AsyncWebServerRequest *request) {
                // Cette route sera gérée par AsyncCallbackJsonWebHandler
            }, nullptr, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                StaticJsonDocument<64> doc;
                DeserializationError error = deserializeJson(doc, (const char*)data);
                
                if (!error && doc["hostname"].is<String>()) {
                    bool success = this->setHostname(doc["hostname"].as<String>());
                    String response = "{\"success\":" + String(success ? "true" : "false") + "}";
                    request->send(200, "application/json", response);
                } else {
                    request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid request\"}");
                }
            });

            // Route pour scanner les réseaux WiFi
            server.on("/api/networks/scan", HTTP_GET, [this](AsyncWebServerRequest *request) {
                JsonObject obj;
                this->getAvailableNetworks(obj);
                String response;
                serializeJson(obj, response);
                request->send(200, "application/json", response);
            });

            // Route pour mettre à jour la configuration AP
            server.on("/api/ap/config", HTTP_POST, [](AsyncWebServerRequest *request) {
                // Cette route sera gérée par AsyncCallbackJsonWebHandler
            }, nullptr, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                StaticJsonDocument<512> doc;
                DeserializationError error = deserializeJson(doc, (const char*)data);
                
                if (!error) {
                    bool success = this->setAPConfig(doc.as<JsonObject>());
                    String response = "{\"success\":" + String(success ? "true" : "false") + "}";
                    request->send(200, "application/json", response);
                } else {
                    request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid request\"}");
                }
            });

            // Route pour mettre à jour la configuration STA
            server.on("/api/sta/config", HTTP_POST, [](AsyncWebServerRequest *request) {
                // Cette route sera gérée par AsyncCallbackJsonWebHandler
            }, nullptr, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                StaticJsonDocument<512> doc;
                DeserializationError error = deserializeJson(doc, (const char*)data);
                
                if (!error) {
                    bool success = this->setSTAConfig(doc.as<JsonObject>());
                    String response = "{\"success\":" + String(success ? "true" : "false") + "}";
                    request->send(200, "application/json", response);
                } else {
                    request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid request\"}");
                }
            });

            // Route pour servir la page de configuration WiFi
            server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
                request->send(SPIFFS, "/wifimanager.html", "text/html");
            });

            // Route pour obtenir le statut des connexions
            _apiServer->addAPI([this](AsyncWebServer& server) {
                server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
                    refreshAPStatus();
                    refreshSTAStatus();

                    StaticJsonDocument<256> doc;
                    JsonObject ap = doc["ap"].to<JsonObject>();
                    apStatus.toJSON(ap);
                    JsonObject sta = doc["sta"].to<JsonObject>();
                    staStatus.toJSON(sta);
                    
                    String response;
                    serializeJson(doc, response);

                    request->send(200, "application/json", response);
                });
            });
        });
    }

    // Ajouter une méthode pour vérifier périodiquement l'état
    void WiFiManager::loop() {
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
            
            // Diffuser le statut
            wsBroadcastStatus();
        }
    }

    // Nouvelle méthode pour gérer les reconnexions
    void WiFiManager::handleReconnections() {
        unsigned long currentTime = millis();
        
        // Gestion du mode Station
        if (staConfig.enabled) {
            if (staStatus.busy) {
                // Vérifier si on est connecté ou en timeout
                if (WiFi.status() == WL_CONNECTED) {
                    staStatus.busy = false;
                    staStatus.connected = true;
                    Serial.println("STA connecté avec succès");
                } 
                else if (currentTime - staStatus.connectionStartTime >= CONNECTION_TIMEOUT) {
                    staStatus.busy = false;
                    staStatus.connected = false;
                    WiFi.disconnect(true);
                    Serial.println("STA timeout de connexion");
                }
            }
            else if (!staStatus.connected) {
                static unsigned long lastSTARetry = 0;
                
                if (currentTime - lastSTARetry >= RETRY_INTERVAL) {
                    Serial.println("Tentative de reconnexion STA...");
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
            Serial.println("Redémarrage du point d'accès...");
            applyAPConfig();
        }
        
        // Gestion du mDNS
        if ((staStatus.connected || apStatus.enabled) && !MDNS.begin(hostname.c_str())) {
            MDNS.end();
            MDNS.begin(hostname.c_str());
        }
    }

    WiFiManager::~WiFiManager() {
        // Déconnexion propre
        WiFi.disconnect(true);
        WiFi.softAPdisconnect(true);
        SPIFFS.end();
    }



    // Helpers

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