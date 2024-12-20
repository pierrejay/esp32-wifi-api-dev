# MQTT API Protocol

## Overview
The MQTT API provides a complete interface to the API server using topic-based messaging. It supports:
- GET and SET operations on API topics
- Event notifications through a dedicated topic
- Automatic API documentation
- JSON payloads for all messages

## Topic Structure

### API Topics
All API operations use the base topic `api/` followed by the method path:
```
api/<path>          # API requests and responses
api                 # API documentation
api/events          # Event notifications
```

## Message Format

### GET Request
```mqtt
Topic: api/wifi/status
Payload: GET
```

Response:
```mqtt
Topic: api/wifi/status
Payload: {
  "ap": {
    "enabled": true,
    "connected": false,
    "clients": 0,
    "ip": "192.168.4.1",
    "rssi": -70
  }
}
```

### SET Request
```mqtt
Topic: api/wifi/sta/config
Payload: SET {
    "enabled": true,
    "network": {
        "ssid": "MyWiFi",
        "password": "12345678"
    }
}
```

Response:
```mqtt
Topic: api/wifi/sta/config
Payload: {
    "success": true
}
```

### Event Notification
```mqtt
Topic: api/events
Payload: {
  "event": "wifi/status",
  "data": {
    "status": {
      "connected": true,
      "ip": "192.168.1.100"
    }
  }
}
```

### Error Response
```mqtt
Topic: api/wifi/sta/config
Payload: {
  "error": "Invalid request"
}
```

## API Documentation
To get the API documentation, send a GET request on the `api` topic:
```mqtt
Topic: api
Payload: GET
```

Response:
```mqtt
Topic: api
Payload: {
  "methods": [{
    "path": "wifi/status",
    "type": "GET",
    "desc": "Get WiFi status",
    "protocols": ["mqtt"],
    "response": {
      "ap": {
        "enabled": "bool",
        "connected": "bool",
        "clients": "int"
      }
    }
  }]
}
```

## Implementation Notes

### Features
- Built on PubSubClient library
- JSON payloads using ArduinoJson
- Automatic reconnection handling
- Event queue for offline resilience
- Automatic API documentation on connection
- Unified topic structure with other protocols (HTTP/WebSocket)

### Limitations
- Maximum JSON payload size: 512 bytes
- Event queue size: 10 messages
- Event processing interval: 50ms
- Reconnection interval: 5s
- Default QoS 0
- No built-in authentication: consider excluding sensitive methods

### Usage Example
```cpp
#include <WiFi.h>
#include <PubSubClient.h>

WiFiClient wifiClient;
APIServer apiServer;
MQTTAPIEndpoint mqttEndpoint(apiServer, wifiClient, "mqtt.broker.com");

void setup() {
    // Connect to WiFi first
    WiFi.begin("ssid", "pass");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }

    // Start MQTT API
    apiServer.addEndpoint(&mqttEndpoint);
    apiServer.begin();
}

void loop() {
    apiServer.poll();  // Handle MQTT connection, messages and events
}
```