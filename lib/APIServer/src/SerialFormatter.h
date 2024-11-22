#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include <map>

class SerialFormatter {
    public:
        static String formatResponse(const String& method, const String& path, const JsonObject& response) {
            String result = method + " " + path + ":";
            bool first = true;
            
            std::function<void(const JsonObject&, const String&)> addParams = 
                [&](const JsonObject& obj, const String& prefix) {
                    for (JsonPair p : obj) {
                        String key = prefix.isEmpty() ? p.key().c_str() 
                                                    : prefix + "." + p.key().c_str();
                        if (p.value().is<JsonObject>()) {
                            addParams(p.value().as<JsonObject>(), key);
                        } else {
                            if (!first) result += ",";
                            result += " " + key + "=";
                            
                            // Handle different types
                            if (p.value().is<bool>())
                                result += p.value().as<bool>() ? "true" : "false";
                            else if (p.value().is<int>())
                                result += String(p.value().as<int>());
                            else if (p.value().is<float>())
                                result += String(p.value().as<float>());
                            else
                                result += p.value().as<String>();
                                
                            first = false;
                        }
                    }
                };
            
            addParams(response, "");
            return result;
        }

        static String formatEvent(const String& event, const JsonObject& data) {
            return "EVT " + event + ": " + formatResponse("EVT", event, data);
        }

        static void parseCommandLine(const String& line, String& method, String& path, std::map<String, String>& params) {
            // Validation de base
            if (line.length() < 4) return;  // Minimum ">GET"
            
            // Supprimer le prompt si présent
            String input = line;
            if (line.startsWith("> ")) {
                input = line.substring(2);
            }
            else if (line.startsWith(">")) {
                input = line.substring(1);
            }
            
            // Analyser la méthode
            int spacePos = input.indexOf(' ');
            if (spacePos == -1) return;
            
            method = input.substring(0, spacePos);
            
            // Parse path
            input = input.substring(spacePos + 1);
            int colonPos = input.indexOf(':');
            
            if (colonPos == -1) {
                // Pas de paramètres
                path = input;
                return;
            }
            
            // A des paramètres
            path = input.substring(0, colonPos);
            String paramsStr = input.substring(colonPos + 1);
            
            // Diviser les paramètres
            int start = 0;
            bool inQuotes = false;
            
            for (size_t i = 0; i < paramsStr.length(); i++) {
                char c = paramsStr[i];
                if (c == '"') inQuotes = !inQuotes;
                else if ((c == ',' || i == paramsStr.length() - 1) && !inQuotes) {
                    // Extraire le paramètre
                    String param = paramsStr.substring(start, i == paramsStr.length() - 1 ? i + 1 : i);
                    param.trim();
                    
                    // Analyser key=value
                    int equalPos = param.indexOf('=');
                    if (equalPos != -1) {
                        String key = param.substring(0, equalPos);
                        String value = param.substring(equalPos + 1);
                        key.trim();
                        value.trim();
                        
                        // Supprimer les guillemets si présents
                        if (value.startsWith("\"") && value.endsWith("\"")) {
                            value = value.substring(1, value.length() - 1);
                        }
                        
                        params[key] = value;
                    }
                    
                    start = i + 1;
                }
            }
        }

        static String formatAPIList(const JsonArray& methods) {
            String response = "\n";
            
            for (JsonVariant method : methods) {
                // Nom de la méthode avec indentation
                response += "    " + method["path"].as<String>() + "\n";
                
                // Propriétés de base
                response += "    ├── type: " + method["type"].as<String>() + "\n";
                response += "    ├── desc: " + method["desc"].as<String>() + "\n";
                
                // Protocols
                response += "    ├── protocols: ";
                JsonArray protocols = method["protocols"].as<JsonArray>();
                bool first = true;
                for (JsonVariant proto : protocols) {
                    if (!first) response += "|";
                    response += proto.as<String>();
                    first = false;
                }
                response += "\n";
                
                // Paramètres si présents
                if (method.containsKey("params")) {
                    response += "    ├── params:\n";
                    JsonObject params = method["params"].as<JsonObject>();
                    int paramCount = params.size();
                    int currentParam = 0;
                    
                    for (JsonPair p : params) {
                        currentParam++;
                        bool isLastParam = (currentParam == paramCount);
                        response += "    │   " + String(isLastParam ? "└── " : "├── ") +
                                   String(p.key().c_str()) + ": " + p.value().as<String>() + "\n";
                    }
                }
                
                // Paramètres de réponse si présents
                if (method.containsKey("response")) {
                    response += "    └── response:\n";
                    JsonObject resp = method["response"].as<JsonObject>();
                    int respCount = resp.size();
                    int currentResp = 0;
                    
                    for (JsonPair p : resp) {
                        currentResp++;
                        bool isLastResp = (currentResp == respCount);
                        response += "        " + String(isLastResp ? "└── " : "├── ") +
                                   String(p.key().c_str()) + ": " + p.value().as<String>() + "\n";
                    }
                }
                
                response += "\n";
            }
            
            return response;
        }

        static String formatError(const String& method, const String& path, const String& error) {
            return method + " " + path + ": error=" + error;
        }
};
