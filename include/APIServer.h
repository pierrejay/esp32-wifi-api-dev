#ifndef API_SERVER_H
#define API_SERVER_H

#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <queue>
#include <string>

#define WS_POLL_INTERVAL 50
#define WS_QUEUE_SIZE 10

class APIServer {
public:
    APIServer(uint16_t port = 80) : _server(port), _ws("/ws"), _lastUpdate(0) {
        _server.addHandler(&_ws);
    }
    
    AsyncWebServer& server() { return _server; }
    
    void begin() {
        _server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
        _server.onNotFound([](AsyncWebServerRequest *request){
            request->send(404, "text/plain", "Page non trouvée");
        });
        _server.begin();
    }

    void textAll(const String& message) {
        _ws.textAll(message);
    }

    void poll() {
        unsigned long now = millis();
        if (now - _lastUpdate > WS_POLL_INTERVAL) {
            processWsQueue();
            _lastUpdate = now;
        }
    }

    void push(const String& message) {
        if (_messageQueue.size() >= WS_QUEUE_SIZE) {
            _messageQueue.pop();
        }
        _messageQueue.push(message);
    }

private:
    AsyncWebServer _server;
    AsyncWebSocket _ws;
    unsigned long _lastUpdate;
    std::queue<String> _messageQueue;

    void processWsQueue() {
        while (!_messageQueue.empty()) {
            textAll(_messageQueue.front());
            _messageQueue.pop();
        }
    }
};

#endif 