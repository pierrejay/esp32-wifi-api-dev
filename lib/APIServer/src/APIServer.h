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
 * @brief Metadata about the API server
 * @brief This structure is used to describe the API server and its capabilities.
 * @brief It is used to generate the API documentation.
 * @brief Only the fields marked as REQUIRED are mandatory (they must be provided in the APIInfo/APIServer constructor).
 * @brief The other fields are optional and some of them are pre-filled with default values (security disabled by default).
 * @brief If you need to customize them, you can do it in your main.cpp file before calling APIServer::begin().
 * @brief Example use with the default WiFiManagerAPI module w/ WebAPIEndpoint :
 * @code
 * #include "WiFiManagerAPI.h"
 * #include "APIServer.h"
 * #include "WebAPIEndpoint.h"
 * 
 * // Declaration of global objects
 * WiFiManager wifiManager;                                    // WiFiManager instance
 * APIServer apiServer({                                       // Master API server
 *     "WiFiManager App API",                                      // title (required)
 *     "1.0.0",                                                    // version (required)
 *     "http://localhost/api"                                  // serverUrl (required)
 * });                                       
 * WiFiManagerAPI wifiManagerAPI(wifiManager, apiServer);      // WiFiManager API interface
 * WebAPIEndpoint webServer(apiServer, 80);                    // Web server endpoint (HTTP+WS)
 * 
 * void setup() {
 *     // Add endpoint routes
 *     apiServer.addEndpoint(&webServer);
 * 
 *     // Define the optional API server metadata
 *     APIInfo& apiInfo = apiServer.getAPIInfo();
 *     apiInfo.description = "WiFi Manager API for ESP32";
 *     apiInfo.contact.name = "John Doe";
 *     apiInfo.contact.email = "john.doe@example.com";
 *     apiInfo.license.name = "MIT";
 *     apiInfo.license.identifier = "MIT";
 * 
 *     // Start the API server
 *     apiServer.begin();
 * }
 * @endcode
 */
struct APIInfo {
    // Required fields
    String title;               // REQUIRED. The title of the API
    String version;             // REQUIRED. The version of the OpenAPI Document

    // Optional fields
    String serverUrl;           // Full server URL where API is accessible (e.g. "http://device.local/api"). Will be set to "/api" if not provided.
    String description;         // A description of the API
    
    // Contact info (optional)
    struct {
        String name;            // Name of contact person/organization
        String email;           // Contact email
    } contact;

    // License info (optional)
    struct {
        String name;           // REQUIRED if license object is present
        String identifier;     // SPDX license identifier
    } license;

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

    // Constructor with required fields
    APIInfo(const String& t, const String& v, const String& url = "/api") : 
        title(t), 
        version(v),
        serverUrl(url), // Mandatory in OpenAPI spec, but can be dismissed (will be set to "/api" if not provided)
        security{false, "", "", "", ""} {}
};

/**
 * @brief Metadata about an API module
 */
struct APIModuleInfo {
    String name;
    String description;
    std::vector<String> routes;
};

/**
 * @brief Type of API methods: GET, SET, EVT (event)
 */
enum class APIMethodType {
    GET,
    SET,
    EVT
};

/**
 * @brief Type of parameters authorized for APIParams (enforces types supported by the OpenAPI spec)
 */
enum class ParamType {
    Boolean,
    Integer,
    Number,
    String,
    Object
};
constexpr const char* paramTypeToString(ParamType type) {
    switch(type) {
        case ParamType::Boolean: return "boolean";
        case ParamType::Integer: return "integer";
        case ParamType::Number: return "number";
        case ParamType::String: return "string";
        case ParamType::Object: return "object";
    }
    return ""; // Pour satisfaire le compilateur
}

/**
 * @brief Parameters of an API method (request or response)
 */
struct APIParam {
    String name;                        // Name of the parameter
    String type;                        // "boolean", "integer", "number", "string", "object"
    bool required = true;               // Default to true (can be dismissed in constructor)
    std::vector<APIParam> properties;   // Stores nested objects (recursive)

    // Constructor to ease initialization
    APIParam(const String& n, ParamType t, bool r = true) 
        : name(n), type(paramTypeToString(t)), required(r) {}

    // Overloaded constructor for recursive nested objects
    APIParam(const String& n, const std::initializer_list<APIParam>& props, bool r = true)
        : name(n), type(paramTypeToString(ParamType::Object)), required(r), properties(props) {}
};

struct APIMethod {
    using Handler = std::function<bool(const JsonObject* args, JsonObject& response)>;
    
    APIMethodType type;                     // GET, SET, EVT (event) 
    Handler handler;                        // Function to execute when the method is called
    String description;                     // Description of the method
    std::vector<APIParam> requestParams;    // Parameters of the request
    std::vector<APIParam> responseParams;   // Parameters of the response
    std::vector<String> exclusions;         // Liste des protocoles exclus
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
    APIMethodBuilder& param(const String& name, ParamType type, bool required = true) {
        _method.requestParams.push_back(APIParam(name, type, required));
        return *this;
    }
    
    // Overloaded param constructor for nested objects
    APIMethodBuilder& param(const String& name, const std::initializer_list<APIParam>& props, bool required = true) {
        _method.requestParams.push_back(APIParam(name, props, required));
        return *this;
    }
    
    // Add a simple response parameter to the method
    APIMethodBuilder& response(const String& name, ParamType type, bool required = true) {
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

    // Eventually, build the method
    APIMethod build() {
        return _method;
    }

private:
    APIMethod _method;  // Stores the method during construction
};

/**
 * @brief Main API Server
 */
class APIServer {
public:
    APIServer(const APIInfo& info) : _apiInfo(info) {}
    
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
     * @brief Register an API module
     * @param name The name of the module
     * @param version The version of the module
     * @param description The description of the module
     */
    void registerModule(const String& name, const String& description) {
        _modules[name] = {name, description, {}};
    }

    /**
     * @brief Register a method to the API server
     * @param path The path of the method
     * @param method The method to register
     */
    void registerMethod(const String& module, const String& path, const APIMethod& method) {
        _methods[path] = method;

        if (_modules.find(module) != _modules.end()) {
            _modules[module].routes.push_back(path);
        }

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
            methodCount++;
            JsonObject methodObj;
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
    std::map<String, APIModuleInfo> _modules;      // Metadata about API modules
    std::vector<APIEndpoint*> _endpoints;          // Objects implementing APIEndpoint
    std::map<String, APIMethod> _methods;          // Registered methods (path/APIMethod map)
    std::map<String, std::vector<String>> _excludedPathsByProtocol; // Excluded paths by protocol

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