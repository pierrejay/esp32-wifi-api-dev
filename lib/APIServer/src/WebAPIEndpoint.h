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

class WebAPIEndpoint : public APIEndpoint {
private:
    std::vector<String> _startupLogs;
    bool _serialReady = false;

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
        setupAPIRoutes();
        setupStaticFiles();
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
        
        _server.on(API_ROUTE, HTTP_GET, [this](AsyncWebServerRequest *request) {
            log("WEBAPI: Requête GET reçue sur /api");
            auto methods = _apiServer.getAPIDoc();
            String response;
            serializeJson(methods, response);
            logf("WEBAPI: Réponse API doc: %s", response.c_str());
            request->send(200, MIME_JSON, response);
        });

        // Routes pour les méthodes GET
        for (const auto& [path, method] : _apiServer.getMethods()) {
            if (method.type == APIMethodType::GET) {
                logf("WEBAPI: Enregistrement route GET /api/%s", path.c_str());
                _server.on(("/api/" + path).c_str(), HTTP_GET, 
                    [this, path](AsyncWebServerRequest *request) {
                        logf("WEBAPI: Requête GET reçue sur /api/%s", path.c_str());
                        handleHTTPGet(request, path);
                    });
            }
        }

        // Routes pour les méthodes SET
        for (const auto& [path, method] : _apiServer.getMethods()) {
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

    void handleHTTPGet(AsyncWebServerRequest* request, const String& path) {
        StaticJsonDocument<1024> doc;
        JsonObject response = doc.to<JsonObject>();
        
        if (_apiServer.executeMethod(path, nullptr, response)) {
            String responseStr;
            serializeJson(doc, responseStr);
            request->send(200, MIME_JSON, responseStr);
        } else {
            request->send(400, MIME_JSON, ERROR_BAD_REQUEST);
        }
    }

    void handleHTTPSet(AsyncWebServerRequest* request, const String& path, 
                      const JsonObject& args) {
        StaticJsonDocument<1024> doc;
        JsonObject response = doc.to<JsonObject>();
        
        if (_apiServer.executeMethod(path, &args, response)) {
            String responseStr;
            serializeJson(doc, responseStr);
            request->send(200, MIME_JSON, responseStr);
        } else {
            request->send(400, MIME_JSON, ERROR_BAD_REQUEST);
        }
    }

    void handleWebSocketMessage(void* arg, uint8_t* data, size_t len) {
        if (!WS_API_ENABLED) return;
        
        AwsFrameInfo* info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && 
            info->opcode == WS_TEXT) {
            
            StaticJsonDocument<1024> doc;
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
        
        StaticJsonDocument<1024> doc;
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
