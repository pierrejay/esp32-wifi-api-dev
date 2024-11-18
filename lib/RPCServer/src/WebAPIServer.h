#ifndef WEB_API_SERVER_H
#define WEB_API_SERVER_H

#include "RPCServer.h"
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <AsyncJson.h>
#include <SPIFFS.h>
#include <queue>

class WebAPIServer : public APIServer {
public:
    WebAPIServer(RPCServer& rpcServer, uint16_t port) 
        : APIServer(rpcServer)
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
    
    ~WebAPIServer() {
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
            processNotificationQueue();
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
            if (_messageQueue.size() >= WS_QUEUE_SIZE) {
                _messageQueue.pop();
            }
            _messageQueue.push(message);
            xSemaphoreGive(_queueMutex);
        }
    }

private:
    AsyncWebServer _server;
    AsyncWebSocket _ws;
    unsigned long _lastUpdate;
    std::queue<String> _messageQueue;
    SemaphoreHandle_t _queueMutex;
    
    static constexpr unsigned long WS_POLL_INTERVAL = 50;
    static constexpr size_t WS_QUEUE_SIZE = 10;
    static constexpr bool WS_RPC_ENABLED = false;  // Désactive explicitement RPC sur WebSocket

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
            auto methods = _rpcServer.getAPIDoc();
            String response;
            serializeJson(methods, response);
            request->send(200, "application/json", response);
        });

        // Routes pour les méthodes GET
        for (const auto& [path, method] : _rpcServer.getMethods()) {
            if (method.type == RPCMethodType::GET) {
                _server.on(("/api/" + path).c_str(), HTTP_GET, 
                    [this, path](AsyncWebServerRequest *request) {
                        handleHTTPGet(request, path);
                    });
            }
        }

        // Routes pour les méthodes SET
        for (const auto& [path, method] : _rpcServer.getMethods()) {
            if (method.type == RPCMethodType::SET) {
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
        
        if (_rpcServer.executeMethod(path, nullptr, response)) {
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
        
        if (_rpcServer.executeMethod(path, &args, response)) {
            String responseStr;
            serializeJson(doc, responseStr);
            request->send(200, "application/json", responseStr);
        } else {
            request->send(400, "application/json", "{\"error\":\"Bad Request\"}");
        }
    }

    void handleWebSocketMessage(void* arg, uint8_t* data, size_t len) {
        if (!WS_RPC_ENABLED) return;
        
        AwsFrameInfo* info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && 
            info->opcode == WS_TEXT) {
            
            StaticJsonDocument<1024> doc;
            DeserializationError error = deserializeJson(doc, (char*)data);
            
            if (!error) {
                JsonObject request = doc.as<JsonObject>();
                if (request.containsKey("method")) {
                    handleRPCRequest(request);
                }
            }
        }
    }

    void handleRPCRequest(const JsonObject& request) {
        if (!WS_RPC_ENABLED) return;
        
        StaticJsonDocument<1024> doc;
        JsonObject response = doc.to<JsonObject>();
        
        String method = request["method"].as<String>();
        JsonObject params = request["params"].as<JsonObject>();
        
        if (_rpcServer.executeMethod(method, &params, response)) {
            String responseStr;
            serializeJson(doc, responseStr);
            _ws.textAll(responseStr);
        }
    }

    void processNotificationQueue() {
        if (xSemaphoreTake(_queueMutex, portMAX_DELAY) == pdTRUE) {
            while (!_messageQueue.empty()) {
                String message = _messageQueue.front();
                _messageQueue.pop();
                _ws.textAll(message);
            }
            xSemaphoreGive(_queueMutex);
        }
    }
};

#endif // WEB_API_SERVER_H
