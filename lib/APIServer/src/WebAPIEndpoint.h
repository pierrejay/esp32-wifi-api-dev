#ifndef WEBAPIENDPOINT_H
#define WEBAPIENDPOINT_H

#include "APIServer.h"
#include "APIEndpoint.h"
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <AsyncJson.h>
#include <SPIFFS.h>
#include <queue>
#include <vector>

#define USE_DYNAMIC_JSON_ALLOC // Uncomment to use dynamic memory allocation for HTTP responses (AsyncJsonResponse)

class WebAPIEndpoint : public APIEndpoint {
private:
    std::vector<String> _startupLogs;
    bool _serialReady = false;

    // Rate limiting
    static constexpr unsigned long REQUEST_MIN_INTERVAL = 100;  // 100ms minimum between requests
    unsigned long _lastRequestTime = 0;

    bool checkRateLimit() {
        unsigned long now = millis();
        if (now - _lastRequestTime < REQUEST_MIN_INTERVAL) {
            return false;
        }
        _lastRequestTime = now;
        return true;
    }

    void log(const String& message) {
        if (!_serialReady) {
            _startupLogs.push_back(message);
        } else {
            Serial.println(message);
        }
    }

    void logf(const char* format, ...) {
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        log(buffer);
    }

public:
    WebAPIEndpoint(APIServer& apiServer, uint16_t port) 
        : APIEndpoint(apiServer)
        , _server(port)
        , _ws(WS_ROUTE)
        , _lastUpdate(0) 
    {
        // Declare supported protocols
        addProtocol("http", GET | SET);
        addProtocol("websocket", EVT);
        
        _server.addHandler(&_ws);
        
        setupWebSocketEvents();
    }
    
    ~WebAPIEndpoint() {
    }

    void begin() override {
        Serial.println("WEBAPI: Setup des routes API...");
        setupAPIRoutes();
        Serial.println("WEBAPI: Setup des fichiers statiques...");
        setupStaticFiles();
        Serial.println("WEBAPI: Démarrage du serveur...");
        _server.begin();
        
        delay(100);
        _serialReady = true;
        
        Serial.println("WEBAPI: Affichage des logs de démarrage:");
        for (const auto& logMessage : _startupLogs) {
            Serial.println(logMessage);
        }
        _startupLogs.clear();
    }

    void poll() override {
        unsigned long now = millis();
        if (now - _lastUpdate > WS_POLL_INTERVAL) {
            processWsQueue();
            _lastUpdate = now;
        }
    }

    void pushEvent(const String& event, const JsonObject& data) override {
        StaticJsonDocument<1024> doc;
        JsonObject eventObj = doc.to<JsonObject>();
        eventObj["event"] = event;
        eventObj["data"] = data;
        
        String message;
        serializeJson(doc, message);
        
        if (_wsQueue.size() >= WS_QUEUE_SIZE) {
            _wsQueue.pop();
        }
        _wsQueue.push(message);
    }

private:
    AsyncWebServer _server;
    AsyncWebSocket _ws;
    unsigned long _lastUpdate;
    std::queue<String> _wsQueue;
    
    static constexpr unsigned long WS_POLL_INTERVAL = 50;
    static constexpr size_t WS_QUEUE_SIZE = 10;
    static constexpr bool WS_API_ENABLED = false;

    static constexpr size_t WS_JSON_BUF = 1024;
    static constexpr size_t GET_JSON_BUF = 2048;
    static constexpr size_t SET_JSON_BUF = 512;
    static constexpr size_t DOC_JSON_BUF = 4096;

    // Constantes pour les routes et types MIME
    static constexpr const char* API_ROUTE = "/api";
    static constexpr const char* WS_ROUTE = "/api/events";
    static constexpr const char* MIME_JSON = "application/json";
    static constexpr const char* MIME_TEXT = "text/plain";
    static constexpr const char* ERROR_BAD_REQUEST = "{\"error\":\"Bad Request\"}";
    static constexpr const char* ERROR_NOT_FOUND = "Not Found";



    void setupWebSocketEvents() {
        _ws.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, 
                          AwsEventType type, void* arg, uint8_t* data, size_t len) {
            if (type == WS_EVT_DATA) {
                handleWebSocketMessage(arg, data, len);
            }
        });
    }

    void setupAPIRoutes() {
        log("WEBAPI: Configuration des routes API...");

        // GET methods (HTTP GET)
        for (const auto& [path, method] : _apiServer.getMethods("http")) {
            if (method.type == APIMethodType::GET) {
                logf("WEBAPI: Enregistrement route GET /api/%s", path.c_str());
                _server.on(("/api/" + path).c_str(), HTTP_GET, 
                    [this, path](AsyncWebServerRequest *request) {
                        logf("WEBAPI: Requête GET reçue sur /api/%s", path.c_str());
                        handleHTTPGet(request, path);
                    });
            }
        }

        // SET methods (HTTP POST : implicitly handled by AsyncCallbackJsonWebHandler)
        for (const auto& [path, method] : _apiServer.getMethods("http")) {
            if (method.type == APIMethodType::SET) {
                logf("WEBAPI: Enregistrement route SET /api/%s", path.c_str());
                auto handler = new AsyncCallbackJsonWebHandler(
                    ("/api/" + path).c_str(),
                    [this, path](AsyncWebServerRequest* request, JsonVariant& json) {
                        logf("WEBAPI: Requête SET reçue sur /api/%s", path.c_str());
                        handleHTTPSet(request, path, json.as<JsonObject>());
                    }
                );
                _server.addHandler(handler);
            }
        }
      
        // API Documentation route (HTTP GET)
        _server.on(API_ROUTE, HTTP_GET, [this](AsyncWebServerRequest *request) {
            log("WEBAPI: Requête GET reçue sur /api");
            handleHTTPDoc(request);
        });
    }

    void setupStaticFiles() {
        _server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
        
        _server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(204);  // No Content
        });
        
        _server.onNotFound([](AsyncWebServerRequest *request) {
            request->send(404, MIME_TEXT, ERROR_NOT_FOUND);
        });
    }

    #ifdef USE_DYNAMIC_JSON_ALLOC

    // GET methods (HTTP GET)
    void handleHTTPGet(AsyncWebServerRequest* request, const String& path) {
        if (!checkRateLimit()) {
            request->send(429, MIME_JSON, "{\"error\":\"Too Many Requests\"}");
            logf("WEBAPI: handleHTTPGet - Requête GET rejetée pour %s (429 Too Many Requests)", path.c_str());
            return;
        }

        logf("WEBAPI: handleHTTPGet - Traitement de la requête GET pour %s", path.c_str());
        
        // Création d'une réponse JSON asynchrone (2KB, mode objet)
        AsyncJsonResponse* response = new AsyncJsonResponse(false, GET_JSON_BUF);
        if (!response) {
            logf("WEBAPI: handleHTTPGet - Erreur d'allocation mémoire pour %s", path.c_str());
            request->send(500, MIME_JSON, "{\"error\":\"Memory allocation failed\"}");
            return;
        }
        
        JsonObject root = response->getRoot();
        
        logf("WEBAPI: handleHTTPGet - Appel de executeMethod pour %s", path.c_str());
        if (_apiServer.executeMethod(path, nullptr, root)) {
            // Debug de la réponse
            String debugResponse;
            serializeJson(root, debugResponse);
            logf("WEBAPI: handleHTTPGet - Réponse générée: %s", debugResponse.c_str());
            
            response->setLength();
            request->send(response);
            logf("WEBAPI: handleHTTPGet - Réponse envoyée avec succès pour %s", path.c_str());
        } else {
            logf("WEBAPI: handleHTTPGet - Erreur lors de l'exécution de la méthode %s", path.c_str());
            delete response;
            request->send(400, MIME_JSON, ERROR_BAD_REQUEST);
        }
    }

    // POST methods (HTTP POST)
    void handleHTTPSet(AsyncWebServerRequest* request, const String& path, 
                      const JsonObject& args) {
        if (!checkRateLimit()) {
            request->send(429, MIME_JSON, "{\"error\":\"Too Many Requests\"}");
            logf("WEBAPI: handleHTTPSet - Requête SET rejetée pour %s (429 Too Many Requests)", path.c_str());
            return;
        }

        logf("WEBAPI: handleHTTPSet - Traitement de la requête SET pour %s", path.c_str());
        
        // Debug des arguments reçus
        String debugArgs;
        serializeJson(args, debugArgs);
        logf("WEBAPI: handleHTTPSet - Arguments reçus: %s", debugArgs.c_str());
        
        // Création d'une réponse JSON asynchrone (512B, mode objet)
        AsyncJsonResponse* response = new AsyncJsonResponse(false, SET_JSON_BUF);
        if (!response) {
            logf("WEBAPI: handleHTTPSet - Erreur d'allocation mémoire pour %s", path.c_str());
            request->send(500, MIME_JSON, "{\"error\":\"Memory allocation failed\"}");
            return;
        }
        
        JsonObject root = response->getRoot();
        
        if (_apiServer.executeMethod(path, &args, root)) {
            // Debug de la réponse
            String debugResponse;
            serializeJson(root, debugResponse);
            logf("WEBAPI: handleHTTPSet - Réponse générée: %s", debugResponse.c_str());
            
            response->setLength();
            request->send(response);
            logf("WEBAPI: handleHTTPSet - Réponse envoyée avec succès pour %s", path.c_str());
        } else {
            logf("WEBAPI: handleHTTPSet - Erreur lors de l'exécution de la méthode %s", path.c_str());
            delete response;  // Important de libérer la mémoire si on n'utilise pas la réponse
            request->send(400, MIME_JSON, ERROR_BAD_REQUEST);
        }
    }

    void handleHTTPDoc(AsyncWebServerRequest* request) {
        // Création d'une réponse JSON asynchrone avec une capacité de 4KB
        AsyncJsonResponse* response = new AsyncJsonResponse(DOC_JSON_BUF);
        JsonArray methods = response->getRoot();
        
        // Génération de la documentation API
        int methodCount = _apiServer.getAPIDoc(methods);
        logf("WEBAPI: Documentation générée pour %d méthodes", methodCount);
        
        // Envoi de la réponse
        response->setLength();
        request->send(response);
    }

    #else

    void handleHTTPGet(AsyncWebServerRequest* request, const String& path) {
        if (!checkRateLimit()) {
            request->send(429, MIME_JSON, "{\"error\":\"Too Many Requests\"}");
            logf("WEBAPI: handleHTTPGet - Requête GET rejetée pour %s (429 Too Many Requests)", path.c_str());
            return;
        }

        logf("WEBAPI: handleHTTPGet - Traitement de la requête GET pour %s", path.c_str());

        // StaticJsonDocument avec buffer alloué statiquement
        StaticJsonDocument<GET_JSON_BUF> doc;
        JsonObject root = doc.to<JsonObject>();

        logf("WEBAPI: handleHTTPGet - Appel de executeMethod pour %s", path.c_str());
        if (_apiServer.executeMethod(path, nullptr, root)) {
            // Debug de la réponse
            char responseBuffer[GET_JSON_BUF];
            serializeJson(doc, responseBuffer, GET_JSON_BUF);
            logf("WEBAPI: handleHTTPGet - Réponse générée: %s", responseBuffer);

            // Envoi de la réponse
            request->send(200, MIME_JSON, responseBuffer);
            logf("WEBAPI: handleHTTPGet - Réponse envoyée avec succès pour %s", path.c_str());
        } else {
            logf("WEBAPI: handleHTTPGet - Erreur lors de l'exécution de la méthode %s", path.c_str());
                request->send(400, MIME_JSON, ERROR_BAD_REQUEST);
            }
    }


    void handleHTTPSet(AsyncWebServerRequest* request, const String& path, const JsonObject& args) {
        if (!checkRateLimit()) {
            request->send(429, MIME_JSON, "{\"error\":\"Too Many Requests\"}");
            logf("WEBAPI: handleHTTPSet - Requête SET rejetée pour %s (429 Too Many Requests)", path.c_str());
            return;
        }

        logf("WEBAPI: handleHTTPSet - Traitement de la requête SET pour %s", path.c_str());

        // Debug des arguments reçus
        String debugArgs;
        serializeJson(args, debugArgs);
        logf("WEBAPI: handleHTTPSet - Arguments reçus: %s", debugArgs.c_str());

        // StaticJsonDocument avec buffer alloué statiquement
        StaticJsonDocument<SET_JSON_BUF> doc;
        JsonObject root = doc.to<JsonObject>();

        if (_apiServer.executeMethod(path, &args, root)) {
            // Debug de la réponse
            char responseBuffer[SET_JSON_BUF];
            serializeJson(doc, responseBuffer, SET_JSON_BUF);
            logf("WEBAPI: handleHTTPSet - Réponse générée: %s", responseBuffer);

            // Envoi de la réponse
            request->send(200, MIME_JSON, responseBuffer);
            logf("WEBAPI: handleHTTPSet - Réponse envoyée avec succès pour %s", path.c_str());
        } else {
            logf("WEBAPI: handleHTTPSet - Erreur lors de l'exécution de la méthode %s", path.c_str());
            request->send(400, MIME_JSON, ERROR_BAD_REQUEST);
            }
    }

    void handleHTTPDoc(AsyncWebServerRequest* request) {
        logf("WEBAPI: handleHTTPDoc - Génération de la documentation API");

        // StaticJsonDocument avec buffer alloué statiquement
        StaticJsonDocument<DOC_JSON_BUF> doc;
        JsonArray methods = doc.to<JsonArray>();

        // Génération de la documentation API
        int methodCount = _apiServer.getAPIDoc(methods);
        logf("WEBAPI: Documentation générée pour %d méthodes", methodCount);

        // Conversion en chaîne JSON pour l'envoi
        char responseBuffer[DOC_JSON_BUF];
        serializeJson(doc, responseBuffer, DOC_JSON_BUF);

        // Envoi de la réponse
        request->send(200, MIME_JSON, responseBuffer);
        logf("WEBAPI: handleHTTPDoc - Réponse envoyée avec succès");
    }

#endif

    void handleWebSocketMessage(void* arg, uint8_t* data, size_t len) {
        if (!WS_API_ENABLED) return;
        
        AwsFrameInfo* info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && 
            info->opcode == WS_TEXT) {
            
            StaticJsonDocument<WS_JSON_BUF> doc;
            DeserializationError error = deserializeJson(doc, (char*)data);
            
            if (!error) {
                JsonObject request = doc.as<JsonObject>();
                if (request.containsKey("method")) {
                    handleAPIRequest(request);
                }
            }
        }
    }

    void handleAPIRequest(const JsonObject& request) {
        if (!WS_API_ENABLED) return;
        
        StaticJsonDocument<WS_JSON_BUF> doc;
        JsonObject response = doc.to<JsonObject>();
        
        String method = request["method"].as<String>();
        JsonObject params = request["params"].as<JsonObject>();
        
        if (_apiServer.executeMethod(method, &params, response)) {
            String responseStr;
            serializeJson(doc, responseStr);
            _ws.textAll(responseStr);
        }
    }

    void processWsQueue() {
        while (!_wsQueue.empty()) {
            String message = _wsQueue.front();
            _wsQueue.pop();
            _ws.textAll(message);
        }
    }
};

#endif // WEBAPIENDPOINT_H
