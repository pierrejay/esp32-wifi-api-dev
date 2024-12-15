#ifndef API_DOC_H
#define API_DOC_H

#include <ArduinoJson.h>
#include "APIServer.h"

/**
 * @brief Générateur de documentation OpenAPI
 */
class APIDocGenerator {
public:
    /**
     * @brief Génère la documentation OpenAPI au format JSON
     * @param apiServer Instance du serveur API
     * @param doc Document JSON de sortie
     * @param fs Système de fichiers pour sauvegarder le JSON
     * @return true si la génération a réussi, false sinon
     */
    static bool generateOpenAPIDocJson(APIServer& apiServer, JsonObject& doc, fs::FS& fs) {
        addBaseInfo(apiServer, doc);
        addServerInfo(apiServer, doc);
        addPaths(apiServer, doc);
        return saveToFile(doc, fs);
    }

private:
    /**
     * @brief Convertit une chaîne en minuscules
     */
    static String toLowerCase(const String& str) {
        String result = str;
        for (char& c : result) {
            c = tolower(c);
        }
        return result;
    }

    /**
     * @brief Ajoute les informations de base OpenAPI
     */
    static void addBaseInfo(APIServer& apiServer, JsonObject& doc) {
        doc["openapi"] = "3.0.0";
        
        JsonObject info = doc["info"].to<JsonObject>();
        info["title"] = apiServer.getApiInfo().title;
        info["version"] = apiServer.getApiInfo().version;
        
        if (!apiServer.getApiInfo().description.isEmpty()) {
            info["description"] = apiServer.getApiInfo().description;
        }
        if (!apiServer.getApiInfo().license.isEmpty()) {
            info["license"]["name"] = apiServer.getApiInfo().license;
        }
        
        addContactInfo(apiServer, info);
        addSecurityInfo(apiServer, doc);
        addLinks(apiServer, info, doc);
        addLifecycleInfo(apiServer, info);
        addDeploymentInfo(apiServer, info);

        // Add global BasicAuth security scheme
        JsonObject securitySchemes = doc["components"]["securitySchemes"].to<JsonObject>();
        JsonObject basicAuth = securitySchemes["BasicAuth"].to<JsonObject>();
        basicAuth["type"] = "http";
        basicAuth["scheme"] = "basic";
    }

    /**
     * @brief Ajoute les informations de contact
     */
    static void addContactInfo(APIServer& apiServer, JsonObject& info) {
        if (!apiServer.getApiInfo().contact.name.isEmpty() || !apiServer.getApiInfo().contact.email.isEmpty()) {
            JsonObject contact = info["contact"].to<JsonObject>();
            if (!apiServer.getApiInfo().contact.name.isEmpty()) {
                contact["name"] = apiServer.getApiInfo().contact.name;
            }
            if (!apiServer.getApiInfo().contact.email.isEmpty()) {
                contact["email"] = apiServer.getApiInfo().contact.email;
            }
        }
    }

    /**
     * @brief Ajoute les informations de sécurité
     */
    static void addSecurityInfo(APIServer& apiServer, JsonObject& doc) {
        if (apiServer.getApiInfo().security.enabled) {
            JsonArray security = doc["security"].to<JsonArray>();
            JsonObject securityScheme = doc["components"]["securitySchemes"][apiServer.getApiInfo().security.type].to<JsonObject>();
            
            securityScheme["type"] = apiServer.getApiInfo().security.type;
            if (!apiServer.getApiInfo().security.scheme.isEmpty()) {
                securityScheme["scheme"] = apiServer.getApiInfo().security.scheme;
            }
            
            if (apiServer.getApiInfo().security.type == "apiKey") {
                securityScheme["name"] = apiServer.getApiInfo().security.keyName;
                securityScheme["in"] = apiServer.getApiInfo().security.keyLocation;
            }
            
            JsonObject requirement = security.add<JsonObject>();
            requirement[apiServer.getApiInfo().security.type] = JsonArray();
        }
    }

    /**
     * @brief Ajoute les liens externes
     */
    static void addLinks(APIServer& apiServer, JsonObject& info, JsonObject& doc) {
        if (!apiServer.getApiInfo().links.termsOfService.isEmpty()) {
            info["termsOfService"] = apiServer.getApiInfo().links.termsOfService;
        }
        if (!apiServer.getApiInfo().links.externalDocs.isEmpty()) {
            JsonObject externalDocs = doc["externalDocs"].to<JsonObject>();
            externalDocs["url"] = apiServer.getApiInfo().links.externalDocs;
        }
    }

    /**
     * @brief Ajoute les informations de cycle de vie
     */
    static void addLifecycleInfo(APIServer& apiServer, JsonObject& info) {
        if (apiServer.getApiInfo().lifecycle.deprecated) {
            info["deprecated"] = true;
            if (!apiServer.getApiInfo().lifecycle.deprecationDate.isEmpty()) {
                info["x-deprecation-date"] = apiServer.getApiInfo().lifecycle.deprecationDate;
            }
            if (!apiServer.getApiInfo().lifecycle.alternativeUrl.isEmpty()) {
                info["x-alternative-url"] = apiServer.getApiInfo().lifecycle.alternativeUrl;
            }
        }
    }

    /**
     * @brief Ajoute les informations de déploiement
     */
    static void addDeploymentInfo(APIServer& apiServer, JsonObject& info) {
        if (!apiServer.getApiInfo().deployment.environment.isEmpty()) {
            info["x-environment"] = apiServer.getApiInfo().deployment.environment;
        }
        if (apiServer.getApiInfo().deployment.beta) {
            info["x-beta"] = true;
        }
        if (!apiServer.getApiInfo().deployment.region.isEmpty()) {
            info["x-region"] = apiServer.getApiInfo().deployment.region;
        }
    }

    /**
     * @brief Ajoute les informations de serveur
     */
    static void addServerInfo(APIServer& apiServer, JsonObject& doc) {
        JsonArray servers = doc["servers"].to<JsonArray>();
        JsonObject server = servers.add<JsonObject>();
        server["url"] = apiServer.getApiInfo().serverUrl;
    }

    /**
     * @brief Ajoute les chemins d'API
     */
    static void addPaths(APIServer& apiServer, JsonObject& doc) {
        JsonObject paths = doc["paths"].to<JsonObject>();
        
        for (const auto& [path, method] : apiServer.getMethods()) {
            // Skip hidden methods
            if (method.hidden) {
                continue;
            }

            JsonObject pathItem = paths["/" + String(path)].to<JsonObject>();
            
            const char* httpMethod = (method.type == APIMethodType::SET) ? "post" : 
                                   (method.type == APIMethodType::GET) ? "get" : 
                                   "websocket";
            
            if (method.type != APIMethodType::EVT) {
                JsonObject operation = pathItem[httpMethod].to<JsonObject>();
                operation["description"] = method.description;
                
                // Add security requirement if basic auth is enabled
                if (method.auth.enabled) {
                    JsonArray security = operation["security"].to<JsonArray>();
                    JsonObject requirement = security.createNestedObject();
                    requirement["BasicAuth"] = JsonArray();
                }

                addTags(path, operation);
                if (method.type == APIMethodType::GET) {
                    addGetParameters(method, operation);
                } else if (method.type == APIMethodType::SET) {
                    addSetRequestBody(method, operation);
                }
                addResponses(method, operation);
            }
        }
    }

    /**
     * @brief Ajoute les tags basés sur le chemin
     */
    static void addTags(const String& path, JsonObject& operation) {
        JsonArray tags = operation["tags"].to<JsonArray>();
        int firstSlash = path.indexOf('/');
        if (firstSlash > 0) {
            tags.add(path.substring(0, firstSlash));
        }
    }

    /**
     * @brief Ajoute les paramètres pour les méthodes GET
     */
    static void addGetParameters(const APIMethod& method, JsonObject& operation) {
        if (!method.requestParams.empty()) {
            JsonArray parameters = operation["parameters"].to<JsonArray>();
            for (const auto& param : method.requestParams) {
                JsonObject parameter = parameters.add<JsonObject>();
                parameter["name"] = param.name;
                parameter["in"] = "query";
                parameter["required"] = param.required;
                parameter["schema"]["type"] = toLowerCase(param.type);
            }
        }
    }

    /**
     * @brief Ajoute le corps de requête pour les méthodes SET
     */
    static void addSetRequestBody(const APIMethod& method, JsonObject& operation) {
        if (!method.requestParams.empty()) {
            JsonObject requestBody = operation["requestBody"].to<JsonObject>();
            requestBody["required"] = true;
            JsonObject content = requestBody["content"]["application/json"].to<JsonObject>();
            JsonObject schema = content["schema"].to<JsonObject>();
            schema["type"] = "object";
            
            addProperties(method.requestParams, schema);
        }
    }

    /**
     * @brief Ajoute les réponses
     */
    static void addResponses(const APIMethod& method, JsonObject& operation) {
        JsonObject responses = operation["responses"].to<JsonObject>();
        JsonObject response200 = responses["200"].to<JsonObject>();
        response200["description"] = "Successful operation";
        JsonObject content = response200["content"]["application/json"].to<JsonObject>();
        JsonObject schema = content["schema"].to<JsonObject>();
        schema["type"] = "object";
        
        addProperties(method.responseParams, schema);
    }

    /**
     * @brief Ajoute les propriétés à un schéma
     */
    static void addProperties(const std::vector<APIParam>& params, JsonObject& schema) {
        JsonObject properties = schema["properties"].to<JsonObject>();
        JsonArray required = schema["required"].to<JsonArray>();
        
        for (const auto& param : params) {
            JsonObject prop = properties[param.name].to<JsonObject>();
            
            if (!param.properties.empty()) {
                prop["type"] = "object";
                JsonObject subProps = prop["properties"].to<JsonObject>();
                for (const auto& subParam : param.properties) {
                    JsonObject subProp = subProps[subParam.name].to<JsonObject>();
                    subProp["type"] = toLowerCase(subParam.type);
                    if (subParam.required) {
                        if (!prop.containsKey("required")) {
                            prop["required"] = JsonArray();
                        }
                        prop["required"].add(subParam.name);
                    }
                }
            } else {
                prop["type"] = toLowerCase(param.type);
            }
            
            if (param.required) {
                required.add(param.name);
            }
        }
    }

    /**
     * @brief Sauvegarde le document JSON dans un fichier
     */
    static bool saveToFile(JsonObject& doc, fs::FS& fs) {
        File file = fs.open("/openapi.json", "w");
        if (!file) {
            Serial.println("Erreur lors de l'ouverture du fichier openapi.json");
            return false;
        }

        if (serializeJson(doc, file) == 0) {
            Serial.println("Erreur lors de l'écriture du fichier openapi.json");
            file.close();
            return false;
        }

        file.close();
        return true;
    }
};

#endif // API_DOC_H