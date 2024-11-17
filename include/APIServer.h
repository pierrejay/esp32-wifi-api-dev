#ifndef API_SERVER_H
#define API_SERVER_H

#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <queue>
#include <map>
#include <SPIFFS.h>

/**
 * @brief Représente une méthode d'API générique (GET ou SET)
 */
struct RPCMethod {
    using RPCHandler = std::function<bool(const JsonObject* args, JsonObject& response)>;
    enum Type { GET, SET };
    
    Type type;
    RPCHandler handler;
    
    bool isGet() const { return type == GET; }
};

/**
 * @brief Serveur API générique
 */
class APIServer {
public:
    APIServer(uint16_t port = 80) : _server(port), _ws("/ws"), _lastUpdate(0) {
        _queueMutex = xSemaphoreCreateMutex();
        _server.addHandler(&_ws);
    }
    
    ~APIServer() {
        if (_queueMutex != NULL) {
            vSemaphoreDelete(_queueMutex);
        }
    }
    
    // Helper pour créer une méthode GET
    void addGetMethod(const String& resource, const String& method, 
                     const RPCMethod::RPCHandler& handler) {
        String path = buildPath(resource, method);
        _methods.emplace(path, RPCMethod{RPCMethod::GET, handler});
    }
    
    // Helper pour créer une méthode SET
    void addSetMethod(const String& resource, const String& method, 
                     const RPCMethod::RPCHandler& handler) {
        String path = buildPath(resource, method);
        _methods.emplace(path, RPCMethod{RPCMethod::SET, handler}); 
    }

    void begin() {
        // Ajouter la route pour lister les méthodes disponibles
        _server.on("/api", HTTP_GET, [this](AsyncWebServerRequest *request) {
            StaticJsonDocument<2048> doc;
            JsonArray methods = doc.to<JsonArray>();
            
            for (const auto& method : _methods) {
                JsonObject methodObj = methods.createNestedObject();
                methodObj["path"] = method.first;
                methodObj["type"] = method.second.isGet() ? "GET" : "SET";
            }
            
            String response;
            serializeJson(doc, response);
            request->send(200, "application/json", response);
        });

        // Enregistrer toutes les routes HTTP
        for (const auto& method : _methods) {
            if (method.second.isGet()) {
                _server.on(method.first.c_str(), HTTP_GET, [this, &method](AsyncWebServerRequest *request) {
                    handleGetRequest(request, method.second);
                });
            } else {
                auto handler = std::unique_ptr<AsyncCallbackJsonWebHandler>(
                    new AsyncCallbackJsonWebHandler(
                        method.first.c_str(),
                        [this, &method](AsyncWebServerRequest* request, JsonVariant& json) {
                            handleSetRequest(request, method.second, json.as<JsonObject>()); // We don't need to check if json is nullptr since AsyncCallbackJsonWebHandler already checks for it
                        }
                    )
                );
                _server.addHandler(handler.release());
            }
        }

        // Servir les fichiers statiques (index.html, etc.)
        _server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
        _server.onNotFound([](AsyncWebServerRequest *request){
            request->send(404, "text/plain", "Page non trouvée");
        });

        // Démarrer le serveur HTTP
        _server.begin();
    }

    void poll() {
        unsigned long now = millis();
        if (now - _lastUpdate > WS_POLL_INTERVAL) {
            processNotificationQueue();
            _lastUpdate = now;
        }
    }

    void push(const String& message) {
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
    std::map<String, RPCMethod> _methods;
    SemaphoreHandle_t _queueMutex;
    static constexpr unsigned long WS_POLL_INTERVAL = 50;
    static constexpr unsigned long WS_QUEUE_SIZE = 10;

    String buildPath(const String& resource, const String& method) {
        return "/api/" + resource + "/" + method;
    }

    void handleGetRequest(AsyncWebServerRequest* request, const RPCMethod& method) {
        StaticJsonDocument<2048> doc;
        JsonObject response = doc.to<JsonObject>();
        
        bool success = method.handler(nullptr, response);
        if (!success) {
            request->send(400, "application/json", "{\"success\": false}");
            return;
        }
        
        String responseStr;
        serializeJson(doc, responseStr);
        request->send(200, "application/json", responseStr);
    }

    void handleSetRequest(AsyncWebServerRequest* request, const RPCMethod& method, 
                         const JsonObject& args) {
        StaticJsonDocument<2048> doc;
        JsonObject response = doc.to<JsonObject>();
        
        bool success = method.handler(&args, response); 
        if (!success) {
            request->send(400, "application/json", "{\"success\": false}");
            return;
        }
        
        String responseStr;
        serializeJson(doc, responseStr);
        request->send(200, "application/json", responseStr);
    }

    void processNotificationQueue() {
        if (xSemaphoreTake(_queueMutex, portMAX_DELAY) == pdTRUE) {
            while (!_messageQueue.empty()) {
                String message = _messageQueue.front();
                _messageQueue.pop();
                xSemaphoreGive(_queueMutex);
                
                textAll(message);
                
                if (!xSemaphoreTake(_queueMutex, portMAX_DELAY)) {
                    break;
                }
            }
            xSemaphoreGive(_queueMutex);
        }
    }

    void textAll(const String& message) {
        _ws.textAll(message);
    }
};

#endif 