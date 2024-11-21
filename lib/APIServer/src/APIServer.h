#ifndef APISERVER_H
#define APISERVER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <map>
#include <memory>
#include "APIEndpoint.h"

// Forward declarations of API endpoints implementations
class WebAPIEndpoint;

/**
 * @brief Type of API methods: GET, SET, EVT (event)
 */
enum class APIMethodType {
    GET,
    SET,
    EVT
};

/**
 * @brief Parameters of an API method (request or response)
 */
struct APIParam {
    String name;
    String type;                        // "bool", "int", "float", "string", "object"...
    bool required = true;               // Default to true (can be dismissed in constructor)
    std::vector<APIParam> properties;   // Stores nested objects (recursive)

    // Constructor to ease initialization
    APIParam(const String& n, const String& t, bool r = true) 
        : name(n), type(t), required(r) {}

    // Overloaded constructor for recursive nested objects
    APIParam(const String& n, const std::initializer_list<APIParam>& props, bool r = true)
        : name(n), type("object"), required(r), properties(props) {}
};

struct APIMethod {
    using Handler = std::function<bool(const JsonObject* args, JsonObject& response)>;
    
    APIMethodType type;
    Handler handler;
    String description;
    std::vector<APIParam> requestParams;
    std::vector<APIParam> responseParams;
};

/**
 * @brief Builder of an APIMethod.
 * @brief APIMethods are registered in the APIServer and can be used by endpoints.
 * @brief Setting properties allows automatic discovery of the API by clients & data validation.
 * @brief All APIMethods declaration are done at compile-time & checked by the compiler.
 */
class APIMethodBuilder {
public:
    // Normal constructor for GET/SET
    APIMethodBuilder(APIMethodType type, APIMethod::Handler handler) {
        _method.type = type;
        _method.handler = handler;
    }
    
    // Overloaded constructor for EVT (no need for handler)
    APIMethodBuilder(APIMethodType type) {
        if (type != APIMethodType::EVT) {
            return;
        }
        _method.type = type;
        _method.handler = [](const JsonObject*, JsonObject&) { return true; }; // Dummy handler for EVT
    }
    
    // Set the description of the method
    APIMethodBuilder& desc(const String& description) {
        _method.description = description;
        return *this;
    }
    
    // Add a simple parameter to the method
    APIMethodBuilder& param(const String& name, const String& type, bool required = true) {
        _method.requestParams.push_back(APIParam(name, type, required));
        return *this;
    }
    
    // Overloaded param constructor for nested objects
    APIMethodBuilder& param(const String& name, const std::initializer_list<APIParam>& props, bool required = true) {
        _method.requestParams.push_back(APIParam(name, props, required));
        return *this;
    }
    
    // Add a simple response parameter to the method
    APIMethodBuilder& response(const String& name, const String& type, bool required = true) {
        _method.responseParams.push_back(APIParam(name, type, required));
        return *this;
    }
    
    // Overloaded response constructor for nested objects
    APIMethodBuilder& response(const String& name, const std::initializer_list<APIParam>& props, bool required = true) {
        _method.responseParams.push_back(APIParam(name, props, required));
        return *this;
    }

    // Eventually, build the method
    APIMethod build() {
        return _method;
    }

private:
    // Stores the method during construction
    APIMethod _method;
};

/**
 * @brief Main API Server
 */
class APIServer {
public:
    /**
     * @brief Initialize all endpoints
     */
    void begin() {
        Serial.println("APISERVER: Démarrage des endpoints...");
        for (APIEndpoint* endpoint : _endpoints) {
            endpoint->begin();
        }
    }

    /**
     * @brief Poll endpoints for client requests
     */
    void poll() {
        for (APIEndpoint* endpoint : _endpoints) {
            endpoint->poll();
        }
    }

    /**
     * @brief Register a method to the API server
     * @param path The path of the method
     * @param method The method to register
     */
    void registerMethod(const String& path, const APIMethod& method) {
        _methods[path] = method;
    }

    /**
     * @brief Execute a method
     * @param path The path of the method
     * @param args The arguments of the method
     * @param response The response of the method
     * @return True if the method has been executed, false otherwise
     */
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

    /**
     * @brief Broadcast an event to all endpoints
     * @param event The event to broadcast
     * @param data The data to broadcast
     */
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

    /**
     * @brief Get the API documentation
     * @return The API documentation
     */
    int getAPIDoc(JsonArray& output) {
        
        // Recursive function to add object parameters in the JSON document
        std::function<void(JsonObject&, const APIParam&)> addObjectParams = 
            [&addObjectParams](JsonObject& obj, const APIParam& param) {
                if (param.type == "object" && !param.properties.empty()) {
                    JsonObject nested = obj.createNestedObject(param.name);
                    for (const auto& prop : param.properties) {
                        if (prop.type == "object") {
                            addObjectParams(nested, prop);
                        } else {
                            nested[prop.name] = prop.required ? prop.type : prop.type + "*";
                        }
                    }
                } else {
                    obj[param.name] = param.required ? param.type : param.type + "*";
                }
            };

        int methodCount = 0; 
        for (const auto& [path, method] : _methods) {
            StaticJsonDocument<2048> doc;
            methodCount++;
            JsonObject methodObj = doc.to<JsonObject>();
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

            // Add the method to the output array
            output.add(methodObj);
        }
        
        Serial.printf("APISERVER: Documentation générée pour %d méthodes\n", methodCount);
        
        return methodCount;
    }

    /**
     * @brief Gives access to the methods registered in the API server
     * @return The methods registered in the API server
     */
    const std::map<String, APIMethod>& getMethods() const {
        return _methods;
    }

    /**
     * @brief Validate the parameters of a method
     * @param method The method called
     * @param args The arguments of incoming request
     * @return True if the required parameters are present, false otherwise
     */
    bool validateParams(const APIMethod& method, const JsonObject* args) {
        if (!args && !method.requestParams.empty()) {
            return false;  // No arguments while expected
        }
        if (args) {
            for (const auto& param : method.requestParams) {
                if (param.required && !args->containsKey(param.name)) {
                    return false;  // Missing required parameter
                }
                // We don't check the internal structure of objects
            }
        }
        return true;
    }

    /**
     * @brief Add an endpoint to the API server (called in setup())
     * @param endpoint The endpoint to add
     */
    void addEndpoint(APIEndpoint* endpoint) {
        _endpoints.push_back(endpoint);
    }

private:
    std::vector<APIEndpoint*> _endpoints;
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