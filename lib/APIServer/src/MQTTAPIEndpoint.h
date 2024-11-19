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
        
        // Generate client ID from ESP MAC address
        uint8_t mac[6];
        WiFi.macAddress(mac);
        snprintf(_clientId, sizeof(_clientId), "ESP32_%02X%02X%02X", mac[3], mac[4], mac[5]);
        
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

    char _clientId[15]; // Sufficient size for "ESP32_" + 6 hex characters

    void reconnect() {
        if (_mqtt.connected()) return;

        if (_mqtt.connect(_clientId)) {
            // Subscribe to API requests
            _mqtt.subscribe((String(API_TOPIC) + "#").c_str());
            _connected = true;
        }
    }

    void handleMessage(char* topic, byte* payload, unsigned int length) {
        String topicStr(topic);
        if (!topicStr.startsWith(API_TOPIC)) return;
        
        // Extract path from topic (remove 'api/' prefix)
        String path = topicStr.substring(strlen(API_TOPIC));
        
        // Convert payload to string
        String message;
        message.reserve(length);
        for (unsigned int i = 0; i < length; i++) {
            message += (char)payload[i];
        }

        // For a simple GET request
        if (message == "GET") {
            StaticJsonDocument<512> responseDoc;
            JsonObject response = responseDoc.to<JsonObject>();
            
            // If it's a request on the api topic, return the doc
            if (path.length() == 0) {
                response = _apiServer.getAPIDoc();
            } else if (_apiServer.executeMethod(path, nullptr, response)) {
                // Otherwise execute the method normally
            } else {
                StaticJsonDocument<64> errorDoc;
                errorDoc["error"] = "Invalid request";
                String errorStr;
                serializeJson(errorDoc, errorStr);
                _mqtt.publish(topic, errorStr.c_str());
                return;
            }
            
            String responseStr;
            serializeJson(response, responseStr);
            _mqtt.publish(topic, responseStr.c_str());
            return;
        }

        // For a SET request with JSON parameters
        if (message.startsWith("SET ")) {
            String jsonStr = message.substring(4); // Skip "SET "
            StaticJsonDocument<512> requestDoc;
            DeserializationError error = deserializeJson(requestDoc, jsonStr);
            if (error) {
                StaticJsonDocument<64> errorDoc;
                errorDoc["error"] = "Invalid JSON";
                String errorStr;
                serializeJson(errorDoc, errorStr);
                _mqtt.publish(topic, errorStr.c_str());
                return;
            }

            // Execute method
            StaticJsonDocument<512> responseDoc;
            JsonObject response = responseDoc.to<JsonObject>();
            
            if (_apiServer.executeMethod(path, &requestDoc.as<JsonObject>(), response)) {
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
            return;
        }

        // If we arrive here, the format is not recognized
        StaticJsonDocument<64> errorDoc;
        errorDoc["error"] = "Invalid format. Use 'GET' or 'SET {params}'";
        String errorStr;
        serializeJson(errorDoc, errorStr);
        _mqtt.publish(topic, errorStr.c_str());
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