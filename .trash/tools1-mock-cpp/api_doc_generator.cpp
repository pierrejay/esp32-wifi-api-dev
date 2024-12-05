#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

using json = nlohmann::json;

// Forward declarations
class APIEndpoint;

// Enums
enum class APIMethodType {
    GET,
    SET,
    EVT
};

enum class ParamType {
    Boolean,
    Integer,
    Number,
    String,
    Object
};

// Convert ParamType to string
std::string paramTypeToString(ParamType type) {
    switch (type) {
        case ParamType::Boolean: return "boolean";
        case ParamType::Integer: return "integer";
        case ParamType::Number: return "number";
        case ParamType::String: return "string";
        case ParamType::Object: return "object";
        default: return "unknown";
    }
}

// Structures
struct APIParam {
    std::string name;
    std::string type;
    bool required = true;
    std::vector<APIParam> properties;

    APIParam(const std::string& n, ParamType t, bool r = true)
        : name(n), type(paramTypeToString(t)), required(r) {}
    
    APIParam(const std::string& n, const std::initializer_list<APIParam>& props, bool r = true)
        : name(n), type(paramTypeToString(ParamType::Object)), required(r), properties(props) {}
};

struct APIMethod {
    using Handler = std::function<bool(const json*, json&)>;
    
    APIMethodType type;
    Handler handler;
    std::string description;
    std::vector<APIParam> requestParams;
    std::vector<APIParam> responseParams;
    std::vector<std::string> exclusions;
};

struct APIModuleInfo {
    std::string name;
    std::string description;
    std::vector<std::string> routes;
};

struct APIInfo {
    std::string title;
    std::string version;
    std::string serverUrl;
    std::string description;
    
    struct {
        std::string name;
        std::string email;
    } contact;

    struct {
        std::string name;
        std::string identifier;
    } license;
};

// Main API Doc Generator class
class APIDocGenerator {
public:
    APIDocGenerator(const APIInfo& info) : apiInfo(info) {}

    void registerModule(const std::string& name, const std::string& description) {
        modules[name] = {name, description, {}};
    }

    void registerMethod(const std::string& module, const std::string& path, const APIMethod& method) {
        methods[path] = method;
        
        if (modules.find(module) != modules.end()) {
            modules[module].routes.push_back(path);
        }
    }

    json generateOpenAPIJson() const {
        json openapi;
        
        // OpenAPI version
        openapi["openapi"] = "3.1.1";
        
        // Info object
        openapi["info"] = {
            {"title", apiInfo.title},
            {"version", apiInfo.version},
            {"description", apiInfo.description}
        };
        
        if (!apiInfo.contact.name.empty()) {
            openapi["info"]["contact"] = {
                {"name", apiInfo.contact.name},
                {"email", apiInfo.contact.email}
            };
        }
        
        if (!apiInfo.license.name.empty()) {
            openapi["info"]["license"] = {
                {"name", apiInfo.license.name},
                {"identifier", apiInfo.license.identifier}
            };
        }

        // Server
        openapi["servers"] = json::array({
            {{"url", apiInfo.serverUrl}}
        });

        // Paths
        json paths;
        for (const auto& [path, method] : methods) {
            json methodObj;
            
            // Operation type
            std::string httpMethod;
            switch (method.type) {
                case APIMethodType::GET: httpMethod = "get"; break;
                case APIMethodType::SET: httpMethod = "post"; break;
                case APIMethodType::EVT: httpMethod = "get"; break; // Events are GET with special handling
            }
            
            // Operation object
            json operation;
            operation["description"] = method.description;
            
            // Parameters
            if (!method.requestParams.empty()) {
                json requestBody;
                json schema = {
                    {"type", "object"},
                    {"required", json::array()}
                };
                
                json properties;
                for (const auto& param : method.requestParams) {
                    if (param.required) {
                        schema["required"].push_back(param.name);
                    }
                    
                    if (param.type == "object") {
                        json objSchema = {
                            {"type", "object"},
                            {"properties", json::object()}
                        };
                        
                        for (const auto& prop : param.properties) {
                            objSchema["properties"][prop.name] = {
                                {"type", prop.type},
                                {"required", prop.required}
                            };
                        }
                        
                        properties[param.name] = objSchema;
                    } else {
                        properties[param.name] = {
                            {"type", param.type}
                        };
                    }
                }
                
                schema["properties"] = properties;
                requestBody["content"] = {
                    {"application/json", {
                        {"schema", schema}
                    }}
                };
                operation["requestBody"] = requestBody;
            }
            
            // Responses
            json responses;
            json successResponse;
            if (!method.responseParams.empty()) {
                json schema = {
                    {"type", "object"},
                    {"required", json::array()}
                };
                
                json properties;
                for (const auto& param : method.responseParams) {
                    if (param.required) {
                        schema["required"].push_back(param.name);
                    }
                    
                    if (param.type == "object") {
                        json objSchema = {
                            {"type", "object"},
                            {"properties", json::object()}
                        };
                        
                        for (const auto& prop : param.properties) {
                            objSchema["properties"][prop.name] = {
                                {"type", prop.type},
                                {"required", prop.required}
                            };
                        }
                        
                        properties[param.name] = objSchema;
                    } else {
                        properties[param.name] = {
                            {"type", param.type}
                        };
                    }
                }
                
                schema["properties"] = properties;
                successResponse["content"] = {
                    {"application/json", {
                        {"schema", schema}
                    }}
                };
            }
            
            successResponse["description"] = "Successful operation";
            responses["200"] = successResponse;
            operation["responses"] = responses;
            
            // Tags (from modules)
            for (const auto& [moduleName, moduleInfo] : modules) {
                if (std::find(moduleInfo.routes.begin(), moduleInfo.routes.end(), path) != moduleInfo.routes.end()) {
                    operation["tags"] = json::array({moduleName});
                    break;
                }
            }
            
            paths["/" + path][httpMethod] = operation;
        }
        
        openapi["paths"] = paths;
        
        // Tags
        json tags;
        for (const auto& [name, info] : modules) {
            tags.push_back({
                {"name", name},
                {"description", info.description}
            });
        }
        openapi["tags"] = tags;
        
        return openapi;
    }

    void generateFiles() const {
        // Generate JSON
        json openapiJson = generateOpenAPIJson();
        std::cout << "Generated JSON:\n" << std::setw(2) << openapiJson << std::endl;
        std::ofstream jsonFile("openapi.json");
        jsonFile << std::setw(2) << openapiJson << std::endl;
        jsonFile.close();
        
        // Generate YAML
        YAML::Node yamlNode;
        yamlNode = YAML::Load(openapiJson.dump());
        YAML::Emitter yamlEmitter;
        yamlEmitter.SetMapFormat(YAML::Block);
        yamlEmitter.SetSeqFormat(YAML::Block);
        yamlEmitter << yamlNode;
        std::ofstream yamlFile("openapi.yaml");
        yamlFile << yamlEmitter.c_str() << std::endl;
        yamlFile.close();
        std::cout << "Generated YAML:\n" << yamlEmitter.c_str() << std::endl;
        
        std::cout << "OpenAPI documentation generated successfully!" << std::endl;
    }

private:
    APIInfo apiInfo;
    std::map<std::string, APIMethod> methods;
    std::map<std::string, APIModuleInfo> modules;
};

// Example usage and WiFi Manager API registration
int main() {
    // Initialize API Info
    APIInfo apiInfo {
        .title = "WiFi Manager API",
        .version = "1.0.0",
        .serverUrl = "http://device.local/api",
        .description = "WiFi configuration and monitoring API",
        .contact = {
            .name = "Pierre",
            .email = ""
        },
        .license = {
            .name = "MIT",
            .identifier = "MIT"
        }
    };
    
    // Create API Doc Generator
    APIDocGenerator generator(apiInfo);
    
    // Register WiFi module
    generator.registerModule("wifi", "WiFi configuration and monitoring");
    
    // Register methods
    
    // GET wifi/status
    APIMethod statusMethod;
    statusMethod.type = APIMethodType::GET;
    statusMethod.description = "Get WiFi status";
    statusMethod.responseParams = {
        APIParam("ap", {
            APIParam("enabled", ParamType::Boolean),
            APIParam("connected", ParamType::Boolean),
            APIParam("clients", ParamType::Integer),
            APIParam("ip", ParamType::String),
            APIParam("rssi", ParamType::Integer)
        }),
        APIParam("sta", {
            APIParam("enabled", ParamType::Boolean),
            APIParam("connected", ParamType::Boolean),
            APIParam("ip", ParamType::String),
            APIParam("rssi", ParamType::Integer)
        })
    };
    generator.registerMethod("wifi", "wifi/status", statusMethod);
    
    // GET wifi/config
    APIMethod configMethod;
    configMethod.type = APIMethodType::GET;
    configMethod.description = "Get WiFi configuration";
    configMethod.responseParams = {
        APIParam("ap", {
            APIParam("enabled", ParamType::Boolean),
            APIParam("ssid", ParamType::String),
            APIParam("password", ParamType::String),
            APIParam("channel", ParamType::Integer),
            APIParam("hidden", ParamType::Boolean)
        }),
        APIParam("sta", {
            APIParam("enabled", ParamType::Boolean),
            APIParam("ssid", ParamType::String),
            APIParam("password", ParamType::String),
            APIParam("hostname", ParamType::String)
        })
    };
    generator.registerMethod("wifi", "wifi/config", configMethod);
    
    // Generate OpenAPI files
    generator.generateFiles();
    
    return 0;
}
