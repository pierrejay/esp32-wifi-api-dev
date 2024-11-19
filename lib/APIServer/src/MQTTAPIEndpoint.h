#ifndef MQTTAPIENDPOINT_H
#define MQTTAPIENDPOINT_H

#include "APIServer.h"
#include "APIEndpoint.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <queue>

class MQTTAPIEndpoint : public APIEndpoint {
public:
    MQTTAPIEndpoint(APIServer& apiServer, Client& client, const char* broker, uint16_t port = 1883) 
        : APIEndpoint(apiServer)
        , _mqtt(client)
        , _broker(broker)
        , _port(port)
        , _lastUpdate(0)
        , _connected(false)
    {
        addProtocol("mqtt", GET | SET | EVT);
        
        _mqtt.setCallback([this](char* topic, byte* payload, unsigned int length) {
            handleMessage(topic, payload, length);
        });
    }

    void begin() override {
        reconnect();
    }

    void poll() override {
        unsigned long now = millis();
        
        // Process MQTT connection
        if (!_mqtt.connected()) {
            if (now - _lastUpdate > RECONNECT_INTERVAL) {
                reconnect();
                _lastUpdate = now;
            }
            return;
        }

        // Process MQTT messages
        _mqtt.loop();

        // Process outgoing events
        if (now - _lastUpdate > EVENT_INTERVAL) {
            processEventQueue();
            _lastUpdate = now;
        }
    }

    void pushEvent(const String& event, const JsonObject& data) override {
        StaticJsonDocument<512> doc;
        JsonObject eventObj = doc.to<JsonObject>();
        eventObj["event"] = event;
        eventObj["data"] = data;
        
        String message;
        serializeJson(doc, message);
        
        if (_eventQueue.size() >= QUEUE_SIZE) {
            _eventQueue.pop();
        }
        _eventQueue.push(message);
    }

private:
    PubSubClient _mqtt;
    const char* _broker;
    uint16_t _port;
    unsigned long _lastUpdate;
    bool _connected;
    std::queue<String> _eventQueue;
    
    static constexpr unsigned long RECONNECT_INTERVAL = 5000;  // 5s between reconnect attempts
    static constexpr unsigned long EVENT_INTERVAL = 50;        // 50ms between event processing
    static constexpr size_t QUEUE_SIZE = 10;
    
    // MQTT Topic structure
    static constexpr const char* API_TOPIC = "api/";          // api/<path>
    static constexpr const char* EVENTS_TOPIC = "api/events"; // api/events

    void reconnect() {
        if (_mqtt.connected()) return;

        if (_mqtt.connect("ESP32_API")) {
            // Subscribe to API requests
            _mqtt.subscribe((String(API_TOPIC) + "#").c_str());
            
            if (!_connected) {
                _connected = true;
                auto methods = _apiServer.getAPIDoc();
                String doc;
                serializeJson(methods, doc);
                _mqtt.publish("api", doc.c_str());
            }
        }
    }

    void handleMessage(char* topic, byte* payload, unsigned int length) {
        String topicStr(topic);
        if (!topicStr.startsWith(API_TOPIC)) return;
        
        // Extract path from topic (remove 'api/' prefix)
        String path = topicStr.substring(strlen(API_TOPIC));
        
        // Special case for API documentation
        if (path.length() == 0) {
            auto methods = _apiServer.getAPIDoc();
            String doc;
            serializeJson(methods, doc);
            _mqtt.publish(topic, doc.c_str());
            return;
        }

        // Parse request
        StaticJsonDocument<512> requestDoc;
        JsonObject args;
        String method;
        bool hasArgs = false;

        if (length > 0) {
            // Convert payload to string
            String message;
            message.reserve(length);
            for (unsigned int i = 0; i < length; i++) {
                message += (char)payload[i];
            }

            DeserializationError error = deserializeJson(requestDoc, message);
            if (error) {
                StaticJsonDocument<64> errorDoc;
                errorDoc["error"] = "Invalid JSON";
                String errorStr;
                serializeJson(errorDoc, errorStr);
                _mqtt.publish(topic, errorStr.c_str());
                return;
            }

            // Extract method and parameters if present
            if (requestDoc.containsKey("method")) {
                method = requestDoc["method"].as<String>();
                if (requestDoc.containsKey("params")) {
                    args = requestDoc["params"].as<JsonObject>();
                    hasArgs = true;
                }
            }
        }
        
        // If no method specified, assume GET
        if (method.length() == 0) {
            method = "GET";
        }

        // Execute method
        StaticJsonDocument<512> responseDoc;
        JsonObject response = responseDoc.to<JsonObject>();
        
        if (_apiServer.executeMethod(path, hasArgs ? &args : nullptr, response)) {
            String responseStr;
            serializeJson(response, responseStr);
            _mqtt.publish(topic, responseStr.c_str());
        } else {
            StaticJsonDocument<64> errorDoc;
            errorDoc["error"] = "Invalid request";
            String errorStr;
            serializeJson(errorDoc, errorStr);
            _mqtt.publish(topic, errorStr.c_str());
        }
    }

    void processEventQueue() {
        while (!_eventQueue.empty() && _mqtt.connected()) {
            if (_mqtt.publish(EVENTS_TOPIC, _eventQueue.front().c_str())) {
                _eventQueue.pop();
            } else {
                break; // Stop if publish fails
            }
        }
    }
};

#endif // MQTTAPIENDPOINT_H