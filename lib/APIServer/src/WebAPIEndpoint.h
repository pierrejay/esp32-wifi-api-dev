#ifndef WEBAPIENDPOINT_H
#define WEBAPIENDPOINT_H

#include "APIServer.h"
#include "APIEndpoint.h"
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <AsyncJson.h>
#include <SPIFFS.h>
#include <queue>

class WebAPIEndpoint : public APIEndpoint {
public:
    WebAPIEndpoint(APIServer& apiServer, uint16_t port) 
        : APIEndpoint(apiServer)
        , _server(port)
        , _ws("/ws")
        , _lastUpdate(0) 
    {
        // Déclarer les protocoles supportés
        addProtocol("http", GET | SET);
        addProtocol("websocket", EVT);
        
        _queueMutex = xSemaphoreCreateMutex();
        _server.addHandler(&_ws);
        
        setupWebSocketEvents();
    }
    
    ~WebAPIEndpoint() {
        if (_queueMutex != NULL) {
            vSemaphoreDelete(_queueMutex);
        }
    }

    void begin() override {
        setupAPIRoutes();
        setupStaticFiles();
        _server.begin();
    }

    void poll() override {
        unsigned long now = millis();
        if (now - _lastUpdate > WS_POLL_INTERVAL) {
            processWsQueue();
            _lastUpdate = now;
        }
    }

    void handleGet(const String& path, const JsonObject* args, JsonObject& response) override {
        // Not used in async web server (handled by handleHTTPGet)
        return;
    }

    void handleSet(const String& path, const JsonObject& args, JsonObject& response) override {
        // Not used in async web server (handled by handleHTTPSet)
        return;
    }

    void pushEvent(const String& event, const JsonObject& data) override {
        StaticJsonDocument<1024> doc;
        JsonObject eventObj = doc.to<JsonObject>();
        eventObj["event"] = event;
        eventObj["data"] = data;
        
        String message;
        serializeJson(doc, message);
        
        if (xSemaphoreTake(_queueMutex, portMAX_DELAY) == pdTRUE) {
            if (_wsQueue.size() >= WS_QUEUE_SIZE) {
                _wsQueue.pop();
            }
            _wsQueue.push(message);
            xSemaphoreGive(_queueMutex);
        }
    }

private:
    AsyncWebServer _server;
    AsyncWebSocket _ws;
    unsigned long _lastUpdate;
    std::queue<String> _wsQueue;
    SemaphoreHandle_t _queueMutex;
    
    static constexpr unsigned long WS_POLL_INTERVAL = 50;
    static constexpr size_t WS_QUEUE_SIZE = 10;
    static constexpr bool WS_API_ENABLED = false;  // Désactive explicitement les appels API sur WebSocket

    void setupWebSocketEvents() {
        _ws.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, 
                          AwsEventType type, void* arg, uint8_t* data, size_t len) {
            if (type == WS_EVT_DATA) {
                handleWebSocketMessage(arg, data, len);
            }
        });
    }

    void setupAPIRoutes() {
        // Route pour la documentation API
        _server.on("/api", HTTP_GET, [this](AsyncWebServerRequest *request) {
            auto methods = _apiServer.getAPIDoc();
            String response;
            serializeJson(methods, response);
            request->send(200, "application/json", response);
        });

        // Routes pour les méthodes GET
        for (const auto& [path, method] : _apiServer.getMethods()) {
            if (method.type == APIMethodType::GET) {
                _server.on(("/api/" + path).c_str(), HTTP_GET, 
                    [this, path](AsyncWebServerRequest *request) {
                        handleHTTPGet(request, path);
                    });
            }
        }

        // Routes pour les méthodes SET
        for (const auto& [path, method] : _apiServer.getMethods()) {
            if (method.type == APIMethodType::SET) {
                auto handler = new AsyncCallbackJsonWebHandler(
                    ("/api/" + path).c_str(),
                    [this, path](AsyncWebServerRequest* request, JsonVariant& json) {
                        handleHTTPSet(request, path, json.as<JsonObject>());
                    }
                );
                _server.addHandler(handler);
            }
        }
    }

    void setupStaticFiles() {
        _server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
        _server.onNotFound([](AsyncWebServerRequest *request) {
            request->send(404, "text/plain", "Not Found");
        });
    }

    void handleHTTPGet(AsyncWebServerRequest* request, const String& path) {
        StaticJsonDocument<1024> doc;
        JsonObject response = doc.to<JsonObject>();
        
        if (_apiServer.executeMethod(path, nullptr, response)) {
            String responseStr;
            serializeJson(doc, responseStr);
            request->send(200, "application/json", responseStr);
        } else {
            request->send(400, "application/json", "{\"error\":\"Bad Request\"}");
        }
    }

    void handleHTTPSet(AsyncWebServerRequest* request, const String& path, 
                      const JsonObject& args) {
        StaticJsonDocument<1024> doc;
        JsonObject response = doc.to<JsonObject>();
        
        if (_apiServer.executeMethod(path, &args, response)) {
            String responseStr;
            serializeJson(doc, responseStr);
            request->send(200, "application/json", responseStr);
        } else {
            request->send(400, "application/json", "{\"error\":\"Bad Request\"}");
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
        if (xSemaphoreTake(_queueMutex, portMAX_DELAY) == pdTRUE) {
            while (!_wsQueue.empty()) {
                String message = _wsQueue.front();
                _wsQueue.pop();
                _ws.textAll(message);
            }
            xSemaphoreGive(_queueMutex);
        }
    }
};

#endif // WEBAPIENDPOINT_H
