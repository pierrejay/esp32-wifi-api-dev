#include <string>
#include <iostream>
#include <map>
#include <vector>
#include <functional>
#include <cstdarg>
#include <algorithm>
#include <fstream>


//##############################################################################
//                             Mock classes
//##############################################################################

// Mock String class - DOIT ÊTRE DÉFINI AVANT APIServer.h
class String : public std::string {
public:
    String() : std::string() {}
    String(const char* str) : std::string(str) {}
    String(const std::string& str) : std::string(str) {}
    
    bool isEmpty() const { return empty(); }
    
    String operator+(const String& other) const {
        return String(std::string(*this) + std::string(other));
    }
    String operator+(const char* other) const {
        return String(std::string(*this) + other);
    }
    
    operator std::string() const { return std::string(*this); }
};

// Mock File System
namespace fs {
    class File {
    public:
        bool print(const char* str) { return true; }
        bool println(const char* str) { return true; }
        void close() {}
        operator bool() { return true; }
        size_t write(uint8_t c) { return 1; }
        size_t write(const uint8_t* buf, size_t size) { return size; }
    };

    class FS {
    public:
        File open(const char* path, const char* mode) { return File(); }
    };
}

// Mock Serial
class SerialMock {
public:
    void println(const char* str) { std::cout << str << std::endl; }
    void printf(const char* format, ...) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
};

// Variables globales mockées
SerialMock Serial;
fs::FS SPIFFS;

// APRÈS tous les mocks, on inclut APIServer
#include <ArduinoJson.h>
#include "../lib/APIServer/src/APIServer.h"

// Fonction utilitaire pour convertir APIMethodType en string
const char* toString(APIMethodType type) {
    switch (type) {
        case APIMethodType::GET: return "GET";
        case APIMethodType::SET: return "SET";
        case APIMethodType::EVT: return "EVT";
        default: return "UNKNOWN";
    }
}

std::string toLowerCase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}


//##############################################################################
//                             API doc generation
//##############################################################################

void dumpRegisteredRoutes(APIServer& apiServer) {
    std::cout << "\nRegistered routes:\n";
    for (const auto& [path, method] : apiServer.getMethods()) {
        std::cout << "  " << path << " [" << toString(method.type) << "]" << std::endl;
        if (!method.description.isEmpty()) {
            std::cout << "    Description: " << std::string(method.description) << std::endl;
        }
    }
}

void dumpRegisteredRoutesAsJson(APIServer& apiServer) {
    JsonDocument doc;
    JsonArray routes = doc.createNestedArray("routes");

    for (const auto& [path, method] : apiServer.getMethods()) {
        JsonObject routeObj = routes.createNestedObject();
        routeObj["path"] = path;
        routeObj["type"] = toString(method.type);
        routeObj["description"] = method.description;

        JsonArray requestParams = routeObj.createNestedArray("requestParams");
        for (const auto& param : method.requestParams) {
            JsonObject paramObj = requestParams.createNestedObject();
            paramObj["name"] = param.name;
            paramObj["type"] = param.type;
            paramObj["required"] = param.required;
        }

        JsonArray responseParams = routeObj.createNestedArray("responseParams");
        for (const auto& param : method.responseParams) {
            JsonObject paramObj = responseParams.createNestedObject();
            paramObj["name"] = param.name;
            paramObj["type"] = param.type;
            paramObj["required"] = param.required;
        }
    }

    std::cout << "\nRegistered routes (JSON):\n";
    serializeJsonPretty(doc, std::cout);
    std::cout << std::endl;
}

void dumpRegisteredRoutesAsOpenAPI(APIServer& apiServer) {
    JsonDocument doc;

    // Info de base OpenAPI 3.0.0
    doc["openapi"] = "3.0.0";
    
    // Info complète
    JsonObject info = doc["info"].to<JsonObject>();
    
    // Required fields
    info["title"] = apiServer.getApiInfo().title;
    info["version"] = apiServer.getApiInfo().version;
    
    // Optional base fields
    if (!apiServer.getApiInfo().description.isEmpty()) info["description"] = apiServer.getApiInfo().description;
    if (!apiServer.getApiInfo().license.isEmpty()) info["license"]["name"] = apiServer.getApiInfo().license;
    
    // Contact info
    if (!apiServer.getApiInfo().contact.name.isEmpty() || !apiServer.getApiInfo().contact.email.isEmpty()) {
        JsonObject contact = info["contact"].to<JsonObject>();
        if (!apiServer.getApiInfo().contact.name.isEmpty()) contact["name"] = apiServer.getApiInfo().contact.name;
        if (!apiServer.getApiInfo().contact.email.isEmpty()) contact["email"] = apiServer.getApiInfo().contact.email;
    }

    // Security info
    if (apiServer.getApiInfo().security.enabled) {
        JsonArray security = doc["security"].to<JsonArray>();
        JsonObject securityScheme = doc["components"]["securitySchemes"][apiServer.getApiInfo().security.type].to<JsonObject>();
        
        securityScheme["type"] = apiServer.getApiInfo().security.type;
        if (!apiServer.getApiInfo().security.scheme.isEmpty()) securityScheme["scheme"] = apiServer.getApiInfo().security.scheme;
        
        if (std::string(apiServer.getApiInfo().security.type) == "apiKey") {
            securityScheme["name"] = apiServer.getApiInfo().security.keyName;
            securityScheme["in"] = apiServer.getApiInfo().security.keyLocation;
        }
        
        // Add security requirement
        JsonObject requirement = security.add<JsonObject>();
        requirement[apiServer.getApiInfo().security.type] = JsonArray();
    }

    // Links
    if (!apiServer.getApiInfo().links.termsOfService.isEmpty()) info["termsOfService"] = apiServer.getApiInfo().links.termsOfService;
    if (!apiServer.getApiInfo().links.externalDocs.isEmpty()) {
        JsonObject externalDocs = doc["externalDocs"].to<JsonObject>();
        externalDocs["url"] = apiServer.getApiInfo().links.externalDocs;
    }

    // Lifecycle
    if (apiServer.getApiInfo().lifecycle.deprecated) {
        info["deprecated"] = true;
        if (!apiServer.getApiInfo().lifecycle.deprecationDate.isEmpty()) {
            info["x-deprecation-date"] = apiServer.getApiInfo().lifecycle.deprecationDate;
        }
        if (!apiServer.getApiInfo().lifecycle.alternativeUrl.isEmpty()) {
            info["x-alternative-url"] = apiServer.getApiInfo().lifecycle.alternativeUrl;
        }
    }

    // Deployment
    if (!apiServer.getApiInfo().deployment.environment.isEmpty()) {
        info["x-environment"] = apiServer.getApiInfo().deployment.environment;
    }
    if (apiServer.getApiInfo().deployment.beta) {
        info["x-beta"] = true;
    }
    if (!apiServer.getApiInfo().deployment.region.isEmpty()) {
        info["x-region"] = apiServer.getApiInfo().deployment.region;
    }

    // Server info
    JsonArray servers = doc["servers"].to<JsonArray>();
    JsonObject server = servers.add<JsonObject>();
    server["url"] = apiServer.getApiInfo().serverUrl;

    // Paths
    JsonObject paths = doc["paths"].to<JsonObject>();
    
    for (const auto& [path, method] : apiServer.getMethods()) {
        JsonObject pathItem = paths["/" + std::string(path)].to<JsonObject>();
        
        // Convertir SET -> post, GET -> get
        const char* httpMethod = (method.type == APIMethodType::SET) ? "post" : 
                                (method.type == APIMethodType::GET) ? "get" : 
                                "websocket";  // EVT devient websocket
        
        if (method.type != APIMethodType::EVT) {
            JsonObject operation = pathItem[httpMethod].to<JsonObject>();
            operation["description"] = method.description;
            JsonArray tags = operation["tags"].to<JsonArray>();
            tags.add("wifi");

            // Parameters pour GET
            if (method.type == APIMethodType::GET && !method.requestParams.empty()) {
                JsonArray parameters = operation["parameters"].to<JsonArray>();
                for (const auto& param : method.requestParams) {
                    JsonObject parameter = parameters.add<JsonObject>();
                    parameter["name"] = param.name;
                    parameter["in"] = "query";
                    parameter["required"] = param.required;
                    parameter["schema"]["type"] = toLowerCase(std::string(param.type));
                }
            }

            // RequestBody pour POST
            if (method.type == APIMethodType::SET && !method.requestParams.empty()) {
                JsonObject requestBody = operation["requestBody"].to<JsonObject>();
                requestBody["required"] = true;
                JsonObject content = requestBody["content"]["application/json"].to<JsonObject>();
                JsonObject schema = content["schema"].to<JsonObject>();
                schema["type"] = "object";
                JsonObject properties = schema["properties"].to<JsonObject>();
                
                JsonArray required = schema["required"].to<JsonArray>();
                for (const auto& param : method.requestParams) {
                    JsonObject prop = properties[param.name].to<JsonObject>();
                    
                    // Si le paramètre a des propriétés, c'est un objet
                    if (!param.properties.empty()) {
                        prop["type"] = "object";
                        JsonObject subProps = prop["properties"].to<JsonObject>();
                        for (const auto& subParam : param.properties) {
                            JsonObject subProp = subProps[subParam.name].to<JsonObject>();
                            subProp["type"] = toLowerCase(std::string(subParam.type));
                        }
                    } else {
                        prop["type"] = toLowerCase(std::string(param.type));
                    }
                    
                    if (param.required) {
                        required.add(param.name);
                    }
                }
            }

            // Responses
            JsonObject responses = operation["responses"].to<JsonObject>();
            JsonObject response200 = responses["200"].to<JsonObject>();
            response200["description"] = "Successful operation";
            JsonObject content = response200["content"]["application/json"].to<JsonObject>();
            JsonObject schema = content["schema"].to<JsonObject>();
            schema["type"] = "object";
            JsonObject properties = schema["properties"].to<JsonObject>();
            
            for (const auto& param : method.responseParams) {
                JsonObject prop = properties[param.name].to<JsonObject>();
                
                // Si le paramètre a des propriétés, c'est un objet
                if (!param.properties.empty()) {
                    prop["type"] = "object";
                    JsonObject subProps = prop["properties"].to<JsonObject>();
                    for (const auto& subParam : param.properties) {
                        JsonObject subProp = subProps[subParam.name].to<JsonObject>();
                        subProp["type"] = toLowerCase(std::string(subParam.type));
                        if (subParam.required) {
                            if (!prop.containsKey("required")) {
                                prop["required"] = JsonArray();
                            }
                            prop["required"].add(subParam.name);
                        }
                    }
                } else {
                    prop["type"] = toLowerCase(std::string(param.type));
                }
                
                if (param.required) {
                    if (!schema.containsKey("required")) {
                        schema["required"] = JsonArray();
                    }
                    schema["required"].add(param.name);
                }
            }
        }
    }

    std::cout << "\nOpenAPI 3.1.1 Specification:\n";
    serializeJsonPretty(doc, std::cout);
    std::cout << std::endl;

    // Sauvegarde dans un fichier dans le dossier tools7
    std::ofstream file("openapi.json");
    if (file.is_open()) {
        serializeJsonPretty(doc, file);
        file.close();
        std::cout << "OpenAPI specification saved to ../openapi.json" << std::endl;
    } else {
        std::cerr << "Error: Could not open ../openapi.json for writing" << std::endl;
    }
}



//##############################################################################
//                           Source code processing
//##############################################################################


void registerAllRoutes(APIServer& _apiServer) {


    /////////////////////////////////////////
    //      START OF API MODULES COPY      //
    /////////////////////////////////////////
    //@APIMODULE_COPY_START
        // API Module name (must be consistent between module info & registerMethod calls)
        const String APIMODULE_NAME = "wifi"; 
       
       // Register API Module metadata (allows to group methods by tags in the documentation)
        _apiServer.registerModuleInfo(
            APIMODULE_NAME,                                 // Name
            "WiFi configuration and monitoring",     // Description
            "1.0.0"                                      // Version
        );

        // GET wifi/status
        _apiServer.registerMethod(APIMODULE_NAME, "wifi/status", 
            APIMethodBuilder(APIMethodType::GET, [](const JsonObject* args, JsonObject& response) { return true; })
            .desc("Get WiFi status")
            .response("ap", {
                {"enabled", APIParamType::Boolean},
                {"connected", APIParamType::Boolean},
                {"clients", APIParamType::Integer},
                {"ip", APIParamType::String},
                {"rssi", APIParamType::Integer}
            })
            .response("sta", {
                {"enabled", APIParamType::Boolean},
                {"connected", APIParamType::Boolean},
                {"ip", APIParamType::String},
                {"rssi", APIParamType::Integer}
            })
            .build()
        );

        // GET wifi/config
        _apiServer.registerMethod(APIMODULE_NAME, "wifi/config",
            APIMethodBuilder(APIMethodType::GET, [](const JsonObject* args, JsonObject& response) { return true; })
            .desc("Get WiFi configuration")
            .response("ap", {
                {"enabled",     APIParamType::Boolean},
                {"ssid",        APIParamType::String},
                {"password",    APIParamType::String},
                {"channel",     APIParamType::Integer},
                {"ip",          APIParamType::String},
                {"gateway",     APIParamType::String},
                {"subnet",      APIParamType::String}
            })
            .response("sta", {
                {"enabled",     APIParamType::Boolean},
                {"ssid",        APIParamType::String},
                {"password",    APIParamType::String},
                {"dhcp",        APIParamType::Boolean},
                {"ip",          APIParamType::String},
                {"gateway",     APIParamType::String},
                {"subnet",      APIParamType::String}
            })
            .build()
        );

        // GET wifi/scan
        _apiServer.registerMethod(APIMODULE_NAME, "wifi/scan",
            APIMethodBuilder(APIMethodType::GET, [](const JsonObject* args, JsonObject& response) { return true; })
            .desc("Scan available WiFi networks")
            .response("networks", {
                {"ssid",        APIParamType::String},
                {"rssi",        APIParamType::Integer},
                {"encryption",  APIParamType::Integer}
            })
            .build()
        );

        // SET wifi/ap/config
        _apiServer.registerMethod(APIMODULE_NAME, "wifi/ap/config",
            APIMethodBuilder(APIMethodType::SET, [](const JsonObject* args, JsonObject& response) { return true; })
            .desc("Configure Access Point")
            .param("enabled",   APIParamType::Boolean)
            .param("ssid",      APIParamType::String)
            .param("password",  APIParamType::String)
            .param("channel",   APIParamType::Integer)
            .param("ip",        APIParamType::String, false)  // Optional
            .param("gateway",   APIParamType::String, false)  // Optional
            .param("subnet",    APIParamType::String, false)  // Optional
            .response("success",APIParamType::Boolean)
            .build()
        );

        // SET wifi/sta/config
        _apiServer.registerMethod(APIMODULE_NAME, "wifi/sta/config",
            APIMethodBuilder(APIMethodType::SET, [](const JsonObject* args, JsonObject& response) { return true; })
            .desc("Configure Station mode")
            .param("enabled",   APIParamType::Boolean)
            .param("ssid",      APIParamType::String)
            .param("password",  APIParamType::String)
            .param("dhcp",      APIParamType::Boolean)
            .param("ip",        APIParamType::String, false)  // Optional
            .param("gateway",   APIParamType::String, false)  // Optional
            .param("subnet",    APIParamType::String, false)  // Optional
            .response("success",APIParamType::Boolean)
            .build()
        );

        // SET wifi/hostname
        _apiServer.registerMethod(APIMODULE_NAME, "wifi/hostname",
            APIMethodBuilder(APIMethodType::SET, [](const JsonObject* args, JsonObject& response) { return true; })
            .desc("Set device hostname")
            .param("hostname",        APIParamType::String)
            .response("success", APIParamType::Boolean)
            .build()
        );

        // EVT wifi/events
        _apiServer.registerMethod(APIMODULE_NAME, "wifi/events",
            APIMethodBuilder(APIMethodType::EVT)
            .desc("WiFi status and configuration updates")
            .response("status", {
                {"ap", {
                    {"enabled",     APIParamType::Boolean},
                    {"connected",   APIParamType::Boolean},
                    {"clients",     APIParamType::Integer},
                    {"ip",          APIParamType::String},
                    {"rssi",        APIParamType::Integer}
                }},
                {"sta", {
                    {"enabled",     APIParamType::Boolean},
                    {"connected",   APIParamType::Boolean},
                    {"ip",          APIParamType::String},
                    {"rssi",        APIParamType::Integer}
                }}
            })
            .response("config", {
                {"ap", {
                    {"enabled",     APIParamType::Boolean},
                    {"ssid",        APIParamType::String},
                    {"password",    APIParamType::String},
                    {"channel",     APIParamType::Integer},
                    {"ip",          APIParamType::String},
                    {"gateway",     APIParamType::String},
                    {"subnet",      APIParamType::String}
                }},
            {"sta", {
                    {"enabled",     APIParamType::Boolean},
                    {"ssid",        APIParamType::String},
                    {"password",    APIParamType::String},
                    {"dhcp",        APIParamType::Boolean},
                    {"ip",          APIParamType::String},
                    {"gateway",     APIParamType::String},
                    {"subnet",      APIParamType::String}
                }}
            })
            .build()
        );
    //@APIMODULE_COPY_END
    /////////////////////////////////////////
    //       END OF API MODULES COPY       //
    /////////////////////////////////////////
}



int main() {

    // Créer les instances
    APIServer apiServer;
    
    //////////////////////////////////////
    //      START OF MAIN.CPP COPY      //
    //////////////////////////////////////
    //@MAIN_COPY_START
    // Register the API metadata
    APIInfo apiInfo;
    apiInfo.title = "WiFiManager API";
    apiInfo.version = "1.0.0";
    apiInfo.description = "WiFi operations control for ESP32";
    apiInfo.serverUrl = "http://esp32.local/api";
    apiInfo.license = "MIT";
    apiInfo.contact.name = "Pierre Jay";
    apiInfo.contact.email = "pierre.jay@gmail.com";
    apiServer.registerAPIInfo(apiInfo);
    //@MAIN_COPY_END
    //////////////////////////////////////
    //        END OF MAIN.CPP COPY      //
    //////////////////////////////////////
    

    // Enregistrer les méthodes de tous les modules
    registerAllRoutes(apiServer);
    
    // Afficher les routes enregistrées
    dumpRegisteredRoutes(apiServer);
    dumpRegisteredRoutesAsOpenAPI(apiServer);
    
    return 0;
}
