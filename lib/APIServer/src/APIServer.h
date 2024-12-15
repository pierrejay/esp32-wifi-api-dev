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



//##############################################################################
//                            Metadata structures
//##############################################################################

/**
 * @brief API server metadata
 */
struct APIInfo {
    // Required base fields
    String title;               // REQUIRED. The title of the API
    String version;             // REQUIRED. The version of the OpenAPI Document

    // Optional base fields
    String serverUrl = "/api";  // Full server URL where API is accessible (e.g. "http://device.local/api"). Will be set to "/api" if not provided.
    String description;         // A description of the API
    String license;             // License name (e.g. "MIT")    
    
    // Contact info (optional)
    struct {
        String name;            // Name of contact person/organization
        String email;           // Contact email
    } contact;

    // Security info (optional)
    struct {
        bool enabled;          // Whether security is enabled
        String type;          // "http", "apiKey", etc.
        String scheme;        // "bearer", "basic", etc.
        String keyName;       // Name of the key for apiKey auth
        String keyLocation;   // "header", "query", "cookie"
    } security;

    // Links (optional)
    struct {
        String termsOfService;     // Terms of service URL
        String externalDocs;       // External documentation URL
    } links;
    
    // Lifecycle (optional)
    struct {
        bool deprecated;           // API deprecated 
        String deprecationDate;    // Deprecation date
        String alternativeUrl;     // Alternative API URL
    } lifecycle;
    
    // Deployment (optional)
    struct {
        String environment;        // dev, staging, prod...
        bool beta;                 // Beta version
        String region;            // Geographic region
    } deployment;

    // Standard HTTP responses - hardcoded 
    static const char* getStandardResponse(const String& code) {
        if (code == "400") return "Bad Request";
        if (code == "401") return "Unauthorized";
        if (code == "403") return "Forbidden";
        if (code == "404") return "Not Found";
        if (code == "405") return "Method Not Allowed";
        if (code == "429") return "Too Many Requests";
        if (code == "500") return "Internal Server Error";
        if (code == "503") return "Service Unavailable";
        return "Unknown Status Code";
    }

};

/**
 * @brief API module (business logic) metadata
 */
struct APIModuleInfo {
    String description;
    String version;
    std::vector<String> routes;
};




//##############################################################################
//                            API method & params types
//##############################################################################

/**
 * @brief Type of API methods: GET, SET, EVT (event)
 */
enum class APIMethodType {
    GET,
    SET,
    EVT
};
constexpr const char* apiMethodTypeToString(APIMethodType type) {
    switch(type) {
        case APIMethodType::GET: return "GET";
        case APIMethodType::SET: return "SET";
        case APIMethodType::EVT: return "EVT";
    }
    return ""; // Pour satisfaire le compilateur
}

/**
 * @brief Type of parameters authorized for APIParams (enforces types supported by the OpenAPI spec)
 */
enum class APIParamType {
    Boolean,
    Integer,
    Number,
    String,
    Object
};
constexpr const char* paramTypeToString(APIParamType type) {
    switch(type) {
        case APIParamType::Boolean: return "boolean";
        case APIParamType::Integer: return "integer";
        case APIParamType::Number: return "number";
        case APIParamType::String: return "string";
        case APIParamType::Object: return "object";
    }
    return ""; // Pour satisfaire le compilateur
}




//##############################################################################
//                             API builder
//##############################################################################

/**
 * @brief Parameters of an API method (request or response)
 */
struct APIParam {
    String name;                        // Name of the parameter
    String type;                        // "boolean", "integer", "number", "string", "object"
    bool required = true;               // Default to true (can be dismissed in constructor)
    std::vector<APIParam> properties;   // Stores nested objects (recursive)

    // Constructor to ease initialization
    APIParam(const String& n, APIParamType t, bool r = true) 
        : name(n), type(paramTypeToString(t)), required(r) {}

    // Overloaded constructor for recursive nested objects
    APIParam(const String& n, const std::initializer_list<APIParam>& props, bool r = true)
        : name(n), type(paramTypeToString(APIParamType::Object)), required(r), properties(props) {}
};

struct APIBasicAuth {
    bool enabled = false;
    String user;
    String password;
};

/**
 * @brief API method data
 */

struct APIMethod {
    using Handler = std::function<bool(const JsonObject* args, JsonObject& response)>;
    
    APIMethodType type;                     // GET, SET, EVT (event) 
    Handler handler;                        // Function to execute when the method is called
    String description;                     // Description of the method
    std::vector<APIParam> requestParams;    // Parameters of the request
    std::vector<APIParam> responseParams;   // Parameters of the response
    std::vector<String> exclusions;         // Liste des protocoles exclus
    bool hidden = false;                    // Si true, la méthode n'apparaît pas dans la doc
    APIBasicAuth auth;                      // Basic auth settings if enabled
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
        _method.handler = [](const JsonObject*, JsonObject&) { return false; }; // Dummy handler for EVT
    }
    
    // Set the description of the method
    APIMethodBuilder& desc(const String& description) {
        _method.description = description;
        return *this;
    }
    
    // Add a simple parameter to the method
    APIMethodBuilder& param(const String& name, APIParamType type, bool required = true) {
        _method.requestParams.push_back(APIParam(name, type, required));
        return *this;
    }
    
    // Overloaded param constructor for nested objects
    APIMethodBuilder& param(const String& name, const std::initializer_list<APIParam>& props, bool required = true) {
        _method.requestParams.push_back(APIParam(name, props, required));
        return *this;
    }
    
    // Add a simple response parameter to the method
    APIMethodBuilder& response(const String& name, APIParamType type, bool required = true) {
        _method.responseParams.push_back(APIParam(name, type, required));
        return *this;
    }
    
    // Overloaded response constructor for nested objects
    APIMethodBuilder& response(const String& name, const std::initializer_list<APIParam>& props, bool required = true) {
        _method.responseParams.push_back(APIParam(name, props, required));
        return *this;
    }

    // Add a protocol to exclude
    APIMethodBuilder& excl(const String& protocol) {
        _method.exclusions.push_back(protocol);
        return *this;
    }

    // Add multiple protocols to exclude
    APIMethodBuilder& excl(const std::initializer_list<String>& protocols) {
        for (const auto& protocol : protocols) {
            _method.exclusions.push_back(protocol);
        }
        return *this;
    }

    // Hide this method from API documentation
    APIMethodBuilder& hide(bool value = true) {
        _method.hidden = value;
        return *this;
    }

    // Add basic authentication to this method
    APIMethodBuilder& basicauth(const String& user, const String& password) {
        _method.auth.enabled = true;
        _method.auth.user = user;
        _method.auth.password = password;
        return *this;
    }

    // Eventually, build the method
    APIMethod build() {
        return _method;
    }

private:
    APIMethod _method;  // Stores the method during construction
};



//##############################################################################
//                             API server definition
//##############################################################################

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
     * @brief Register the API metadata (from parameters)
     * @param title The title of the API
     * @param version The version of the API
     * @param serverUrl The URL of the API server (optional, default to "/api")
     */
    void registerAPIInfo(const String& title, const String& version, const String& serverUrl = "/api") {
        _apiInfo.title = title;
        _apiInfo.version = version;
        _apiInfo.serverUrl = serverUrl;
    }

    /**
     * @brief Register the API metadata (from a structure)
     * @param apiInfo The API metadata structure
     */
    void registerAPIInfo(const APIInfo& apiInfo) {
        _apiInfo = apiInfo;
    }

    /**
     * @brief Register an API module metadata
     * @param name The name of the module
     * @param description The description of the module
     * @param version The version of the module
     */
    void registerModuleInfo(const String& name, const String& description, const String& version = "") {
        _modules[name] = {description, version};
    }

    /**
     * @brief Register a method to the API server
     * @param module The name of the module
     * @param path The path of the method
     * @param method The method to register
     */
    void registerMethod(const String& module, const String& path, const APIMethod& method) {
        // Register the method
        _methods[path] = method;

        // Add the route to module metadata
        auto it = _modules.find(module);
        if (it != _modules.end()) {
            it->second.routes.push_back(path);
        }

        // Add protocol exclusions
        if (!method.exclusions.empty()) {
            for (const auto& excl : method.exclusions) {
                _excludedPathsByProtocol[excl].push_back(path);
            }
        }
    }

    /**
     * @brief Execute a method
     * @param protocol The protocol of the client (used to check if the method is excluded)
     * @param path The path of the method
     * @param args The arguments of the method
     * @param response The response of the method
     * @return True if the method has been executed, false otherwise
     */
    bool executeMethod(const String& protocol, const String& path, const JsonObject* args, JsonObject& response) const {
        // Check first if the method is excluded for this protocol
        auto excludedPaths = _excludedPathsByProtocol.find(protocol);
        if (excludedPaths != _excludedPathsByProtocol.end() && 
            std::find(excludedPaths->second.begin(), excludedPaths->second.end(), path) != excludedPaths->second.end()) {
            return false;  // Méthode exclue pour ce protocole
        }

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
                // Check if the event is not excluded for this protocol
                auto excludedPaths = _excludedPathsByProtocol.find(proto.name);
                if (excludedPaths != _excludedPathsByProtocol.end() && 
                    std::find(excludedPaths->second.begin(), excludedPaths->second.end(), event) != excludedPaths->second.end()) {
                    continue;
                }
                // Check if the protocol supports events
                if (proto.capabilities & APIEndpoint::EVT) {
                    endpoint->pushEvent(event, data);
                    continue;
                }
            }
        }
    }

    /**
     * @brief Get the API documentation
     * @return The API documentation
     */
    int getAPIDoc(JsonArray& output) {
        
        // Recursive lambda to add object parameters in the JSON document
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
            if (method.hidden) {
                continue;  // Skip hidden methods
            }
            methodCount++;
            JsonObject methodObj;
            methodObj["path"] = path;
            methodObj["type"] = apiMethodTypeToString(method.type);
            methodObj["desc"] = method.description;
            
            // Add basic auth info if enabled
            if (method.auth.enabled) {
                methodObj["basicauth"] = true;
            }
            
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
                    // Check if the protocol is supported and not excluded
                    bool isSupported = (proto.capabilities & requiredCap);
                    bool isExcluded = false;
                    for (const auto& excl : method.exclusions) {
                        if (excl == proto.name) {
                            isExcluded = true;
                            break;
                        }
                    }
                    if (isSupported && !isExcluded) {
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
     * @brief Get the methods registered in the API server, optionally filtered by protocol
     * @param protocol The protocol to filter by (optional : empty to get all methods)
     * @return The methods (filtered by protocol if specified)
     */
    const std::map<String, APIMethod> getMethods(const String& protocol = "") const {
        if (protocol.isEmpty()) {
            return _methods;  // Return all methods if no protocol is specified
        }

        // Copy all methods and remove excluded ones
        std::map<String, APIMethod> filteredMethods = _methods;
        auto excludedPaths = _excludedPathsByProtocol.find(protocol);
        if (excludedPaths != _excludedPathsByProtocol.end()) {
            for (const auto& path : excludedPaths->second) {
                filteredMethods.erase(path);
            }
        }
        return filteredMethods;
    }

    /**
     * @brief Get the modules registered in the API server
     * @return The modules
     */
    const std::map<String, APIModuleInfo>& getModules() const {
        return _modules;
    }

    /**
     * @brief Get the API metadata
     * @return The API metadata
     */
    const APIInfo& getApiInfo() const {
        return _apiInfo;
    }

    /**
     * @brief Add an endpoint to the API server (called in setup())
     * @param endpoint The endpoint to add
     */
    void addEndpoint(APIEndpoint* endpoint) {
        _endpoints.push_back(endpoint);
    }

    /**
     * @brief Allows to access and modify the API metadata
     * @return A reference to the APIInfo structure
     */
    APIInfo& getAPIInfo() {
        return _apiInfo;
    }


private:
    APIInfo _apiInfo;                              // Metadata about the API
    std::map<String, APIModuleInfo> _modules;      // API module metadata (includes list of routes)
    std::map<String, APIMethod> _methods;          // Registered methods by path
    std::vector<APIEndpoint*> _endpoints;          // Objects implementing APIEndpoint
    std::map<String, std::vector<String>> _excludedPathsByProtocol; // Excluded paths by protocol


    /**
     * @brief Validate the parameters of a method
     * @param method The method called
     * @param args The arguments of incoming request
     * @return True if the required parameters are present, false otherwise
     */
    bool validateParams(const APIMethod& method, const JsonObject* args) const {
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


};

#endif // APISERVER_H