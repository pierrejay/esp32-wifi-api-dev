#ifndef APISERVER_H
#define APISERVER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <map>
#include <memory>
#include "APIEndpoint.h"

// Forward declarations
class WebAPIEndpoint;

enum class APIMethodType {
    GET,
    SET,
    EVT
};

// Structure describing an API parameter
struct APIParam {
    String name;
    String type;     // "bool", "int", "float", "string", "obj"
    bool required = true;  // Par défaut à true
    std::vector<APIParam> properties;  // Pour les objets imbriqués

    // Constructeur pour faciliter l'initialisation
    APIParam(const String& n, const String& t, bool r = true) 
        : name(n), type(t), required(r) {}

    // Nouveau constructeur pour les objets imbriqués
    APIParam(const String& n, const std::initializer_list<APIParam>& props, bool r = true)
        : name(n), type("obj"), required(r), properties(props) {}
};

// Structure describing an API method
struct APIMethod {
    using Handler = std::function<bool(const JsonObject* args, JsonObject& response)>;
    
    APIMethodType type;
    Handler handler;
    String description;
    std::vector<APIParam> requestParams;
    std::vector<APIParam> responseParams;
};

// Builder for APIMethod
class APIMethodBuilder {
public:
    // Constructeur normal pour GET/SET
    APIMethodBuilder(APIMethodType type, APIMethod::Handler handler) {
        _method.type = type;
        _method.handler = handler;
    }
    
    // Constructeur surchargé pour EVT (pas besoin de handler)
    APIMethodBuilder(APIMethodType type) {
        if (type != APIMethodType::EVT) {
            return;
        }
        _method.type = type;
        _method.handler = [](const JsonObject*, JsonObject&) { return true; }; // Dummy handler for EVT
    }
    
    APIMethodBuilder& desc(const String& description) {
        _method.description = description;
        return *this;
    }
    
    APIMethodBuilder& param(const String& name, const String& type, bool required = true) {
        _method.requestParams.push_back(APIParam(name, type, required));
        return *this;
    }
    
    APIMethodBuilder& param(const String& name, const std::initializer_list<APIParam>& props, bool required = true) {
        _method.requestParams.push_back(APIParam(name, props, required));
        return *this;
    }
    
    APIMethodBuilder& response(const String& name, const String& type, bool required = true) {
        _method.responseParams.push_back(APIParam(name, type, required));
        return *this;
    }
    
    APIMethodBuilder& response(const String& name, const std::initializer_list<APIParam>& props, bool required = true) {
        _method.responseParams.push_back(APIParam(name, props, required));
        return *this;
    }

    
    APIMethod build() {
        return _method;
    }

private:
    APIMethod _method;
};

// Main API Server
class APIServer {
public:
    void begin() {
        for (APIEndpoint* endpoint : _endpoints) {
            endpoint->begin();
        }
    }

    void poll() {
        for (APIEndpoint* endpoint : _endpoints) {
            endpoint->poll();
        }
    }

    void registerMethod(const String& path, const APIMethod& method) {
        _methods[path] = method;
    }

    bool executeMethod(const String& path, const JsonObject* args, JsonObject& response) {
        auto it = _methods.find(path);
        if (it != _methods.end()) {
            if (!validateParams(it->second, args)) {
                return false;
            }
            return it->second.handler(args, response);
        }
        return false;
    }

    void broadcast(const String& event, const JsonObject& data) {
        for (APIEndpoint* endpoint : _endpoints) {
            for (const auto& proto : endpoint->getProtocols()) {
                if (proto.capabilities & APIEndpoint::EVT) {
                    endpoint->pushEvent(event, data);
                    break;
                }
            }
        }
    }

    JsonArray getAPIDoc() {
        StaticJsonDocument<2048> doc;
        JsonArray methods = doc.to<JsonArray>();
        
        std::function<void(JsonObject&, const APIParam&)> addObjectParams = 
            [&addObjectParams](JsonObject& obj, const APIParam& param) {
                if (param.type == "obj" && !param.properties.empty()) {
                    JsonObject nested = obj.createNestedObject(param.name);
                    for (const auto& prop : param.properties) {
                        if (prop.type == "obj") {
                            addObjectParams(nested, prop);  // Maintenant OK
                        } else {
                            nested[prop.name] = prop.required ? prop.type : prop.type + "*";
                        }
                    }
                } else {
                    obj[param.name] = param.required ? param.type : param.type + "*";
                }
            };

        for (const auto& [path, method] : _methods) {
            JsonObject methodObj = methods.createNestedObject();
            methodObj["path"] = path;
            methodObj["type"] = toString(method.type);
            methodObj["desc"] = method.description;
            
            // Add supported protocols
            JsonArray protocols = methodObj.createNestedArray("protocols");
            for (const auto& endpoint : _endpoints) {
                for (const auto& proto : endpoint->getProtocols()) {
                    uint8_t requiredCap;
                    switch (method.type) {
                        case APIMethodType::GET: requiredCap = APIEndpoint::GET; break;
                        case APIMethodType::SET: requiredCap = APIEndpoint::SET; break;
                        case APIMethodType::EVT: requiredCap = APIEndpoint::EVT; break;
                    }
                    if (proto.capabilities & requiredCap) {
                        protocols.add(proto.name);
                    }
                }
            }

            // Add parameters as object
            if (!method.requestParams.empty()) {
                JsonObject params = methodObj.createNestedObject("params");
                for (const auto& param : method.requestParams) {
                    addObjectParams(params, param);
                }
            }

            // Add response parameters as object
            if (!method.responseParams.empty()) {
                JsonObject response = methodObj.createNestedObject("response");
                for (const auto& param : method.responseParams) {
                    addObjectParams(response, param);
                }
            }
        }
        
        return methods;
    }

    // Donne accès aux méthodes enregistrées (en lecture seule)
    const std::map<String, APIMethod>& getMethods() const {
        return _methods;
    }

    bool validateParams(const APIMethod& method, const JsonObject* args) {
        if (!args && !method.requestParams.empty()) {
            return false;  // Pas d'arguments alors qu'on en attend
        }
        if (args) {
            for (const auto& param : method.requestParams) {
                if (param.required && !args->containsKey(param.name)) {
                    return false;  // Paramètre requis manquant
                }
                // On ne vérifie pas la structure interne des objets
            }
        }
        return true;
    }


    void addEndpoint(APIEndpoint* endpoint) {
        _endpoints.push_back(endpoint);
    }

private:
    std::vector<APIEndpoint*> _endpoints;  // Simple vecteur de pointeurs
    std::map<String, APIMethod> _methods;

    static String toString(APIMethodType type) {
        switch (type) {
            case APIMethodType::GET: return "GET";
            case APIMethodType::SET: return "SET";
            case APIMethodType::EVT: return "EVT";
            default: return "UNKNOWN";
        }
    }
};

#endif // APISERVER_H