#ifndef RPC_SERVER_H
#define RPC_SERVER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <map>
#include <memory>
#include "WebAPIServer.h"

// Forward declarations
class APIServer;
class RPCServer;
class WebAPIServer;

enum class RPCMethodType {
    GET,
    SET,
    EVT
};

// Structure describing an RPC parameter
struct RPCParam {
    String name;
    String type;     // "bool", "int", "float", "string", "obj"
    bool required = true;  // Par défaut à true
    std::vector<RPCParam> properties;  // Pour les objets imbriqués

    // Constructeur pour faciliter l'initialisation
    RPCParam(const String& n, const String& t, bool r = true) 
        : name(n), type(t), required(r) {}

    // Nouveau constructeur pour les objets imbriqués
    RPCParam(const String& n, const std::initializer_list<RPCParam>& props, bool r = true)
        : name(n), type("obj"), required(r), properties(props) {}
};

// Structure describing an RPC method
struct RPCMethod {
    using Handler = std::function<bool(const JsonObject* args, JsonObject& response)>;
    
    RPCMethodType type;
    Handler handler;
    String description;
    std::vector<RPCParam> requestParams;
    std::vector<RPCParam> responseParams;
};

// Builder for RPCMethod
class RPCMethodBuilder {
public:
    // Constructeur normal pour GET/SET
    RPCMethodBuilder(RPCMethodType type, RPCMethod::Handler handler) {
        _method.type = type;
        _method.handler = handler;
    }
    
    // Constructeur surchargé pour EVT (pas besoin de handler)
    RPCMethodBuilder(RPCMethodType type) {
        if (type != RPCMethodType::EVT) {
            return;
        }
        _method.type = type;
        _method.handler = [](const JsonObject*, JsonObject&) { return true; }; // Dummy handler for EVT
    }
    
    RPCMethodBuilder& desc(const String& description) {
        _method.description = description;
        return *this;
    }
    
    RPCMethodBuilder& param(const String& name, const String& type, bool required = true) {
        _method.requestParams.push_back(RPCParam(name, type, required));
        return *this;
    }
    
    RPCMethodBuilder& param(const String& name, const std::initializer_list<RPCParam>& props, bool required = true) {
        _method.requestParams.push_back(RPCParam(name, props, required));
        return *this;
    }
    
    RPCMethodBuilder& response(const String& name, const String& type, bool required = true) {
        _method.responseParams.push_back(RPCParam(name, type, required));
        return *this;
    }
    
    RPCMethodBuilder& response(const String& name, const std::initializer_list<RPCParam>& props, bool required = true) {
        _method.responseParams.push_back(RPCParam(name, props, required));
        return *this;
    }

    
    RPCMethod build() {
        return _method;
    }

private:
    RPCMethod _method;
};

// Base class for all API Servers
class APIServer {
public:
    enum Capability {
        GET = 1 << 0,
        SET = 1 << 1,
        EVT = 1 << 2
    };

    struct Protocol {
        String name;
        uint8_t capabilities;
    };

    APIServer(RPCServer& rpcServer) : _rpcServer(rpcServer) {}
    virtual ~APIServer() = default;

    virtual void begin() = 0;
    virtual void poll() = 0;
    
    virtual void handleGet(const String& path, const JsonObject* args, JsonObject& response) = 0;
    virtual void handleSet(const String& path, const JsonObject& args, JsonObject& response) = 0;
    virtual void pushEvent(const String& event, const JsonObject& data) = 0;

    const std::vector<Protocol>& getProtocols() const { return _protocols; }

protected:
    void addProtocol(const String& name, uint8_t capabilities) {
        _protocols.push_back({name, capabilities});
    }

    std::vector<Protocol> _protocols;
    RPCServer& _rpcServer;
};

// Main RPC Server
class RPCServer {
public:
    WebAPIServer& createWebServer(uint16_t port) {
        std::unique_ptr<WebAPIServer> server(new WebAPIServer(*this, port));
        auto& ref = *server;
        _servers.push_back(std::unique_ptr<APIServer>(server.release()));
        return ref;
    }

    void begin() {
        for (const auto& server : _servers) {
            server->begin();
        }
    }

    void poll() {
        for (const auto& server : _servers) {
            server->poll();
        }
    }

    void registerMethod(const String& path, const RPCMethod& method) {
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
        for (const auto& server : _servers) {
            for (const auto& proto : server->getProtocols()) {
                if (proto.capabilities & APIServer::EVT) {
                    server->pushEvent(event, data);
                    break; // One broadcast per server is enough
                }
            }
        }
    }

    JsonArray getAPIDoc() {
        StaticJsonDocument<2048> doc;
        JsonArray methods = doc.to<JsonArray>();
        
        for (const auto& [path, method] : _methods) {
            JsonObject methodObj = methods.createNestedObject();
            methodObj["path"] = path;
            methodObj["type"] = toString(method.type);
            methodObj["desc"] = method.description;
            
            // Add supported protocols
            JsonArray protocols = methodObj.createNestedArray("protocols");
            for (const auto& server : _servers) {
                for (const auto& proto : server->getProtocols()) {
                    uint8_t requiredCap;
                    switch (method.type) {
                        case RPCMethodType::GET: requiredCap = APIServer::GET; break;
                        case RPCMethodType::SET: requiredCap = APIServer::SET; break;
                        case RPCMethodType::EVT: requiredCap = APIServer::EVT; break;
                    }
                    if (proto.capabilities & requiredCap) {
                        protocols.add(proto.name);
                    }
                }
            }

            // Fonction récursive pour ajouter les paramètres d'un objet
            auto addObjectParams = [](JsonObject& obj, const RPCParam& param) {
                if (param.type == "obj" && !param.properties.empty()) {
                    JsonObject nested = obj.createNestedObject(param.name);
                    for (const auto& prop : param.properties) {
                        if (prop.type == "obj") {
                            addObjectParams(nested, prop);  // Récursion
                        } else {
                            nested[prop.name] = prop.required ? prop.type : prop.type + "*";
                        }
                    }
                } else {
                    obj[param.name] = param.required ? param.type : param.type + "*";
                }
            };

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
    const std::map<String, RPCMethod>& getMethods() const {
        return _methods;
    }

    bool validateParams(const RPCMethod& method, const JsonObject* args) {
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

private:
    std::vector<std::unique_ptr<APIServer>> _servers;
    std::map<String, RPCMethod> _methods;

    static String toString(RPCMethodType type) {
        switch (type) {
            case RPCMethodType::GET: return "GET";
            case RPCMethodType::SET: return "SET";
            case RPCMethodType::EVT: return "EVT";
            default: return "UNKNOWN";
        }
    }
};

#endif // RPC_SERVER_H