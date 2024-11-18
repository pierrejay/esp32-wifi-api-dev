Voici votre code HTML converti en un fichier Markdown compatible avec GitHub :

```markdown
# APIServer Documentation

## Overview

APIServer is a library designed for ESP32 that simplifies API creation and management.  
It provides a clear separation between business logic, API server management, and protocol-specific endpoints.

## Key Features

- Protocol-agnostic design (HTTP, WebSocket, etc.)
- Intuitive route declaration with builder pattern
- Automatic API documentation
- Multiple protocol support
- Push event system
- Parameter type validation

## Architecture

### Main Components

| Component       | Description                              |
|-----------------|------------------------------------------|
| APIServer       | Central orchestrator managing methods and endpoints |
| APIEndpoint     | Abstract class for protocol implementations |
| WebAPIEndpoint  | HTTP/WebSocket implementation           |

### Method Types

| Type | Usage             | Example                |
|------|-------------------|------------------------|
| GET  | Read-only         | Get WiFi status        |
| SET  | State modification| Configure WiFi         |
| EVT  | Push notifications| WiFi state changes     |

## Implementation Example with WiFiManager

### 1. Initial Setup

```cpp
// In main.cpp
#include "WiFiManager.h"
#include "WiFiManagerAPI.h"
#include "APIServer.h"
#include "WebAPIEndpoint.h"

WiFiManager wifiManager;
APIServer apiServer;
WiFiManagerAPI wifiManagerAPI(wifiManager, apiServer);
WebAPIEndpoint webServer(apiServer, 80);

void setup() {
    // Add web endpoint
    apiServer.addEndpoint(&webServer);
    
    // Initialization
    wifiManager.begin();
    apiServer.begin();
}

void loop() {
    wifiManager.poll();
    wifiManagerAPI.poll();
    apiServer.poll();
}
```

### 2. API Methods Definition

```cpp
// In WiFiManagerAPI.h
void WiFiManagerAPI::registerMethods() {
    // GET method for status
    _apiServer.registerMethod("wifi/status", 
        APIMethodBuilder(APIMethodType::GET, [this](const JsonObject* args, JsonObject& response) {
            _wifiManager.getStatusToJson(response);
            return true;
        })
        .desc("Get WiFi status")
        .response("ap", {
            {"enabled", "bool"},
            {"connected", "bool"},
            {"clients", "int"}
        })
        .response("sta", {
            {"enabled", "bool"},
            {"connected", "bool"},
            {"rssi", "int"}
        })
        .build()
    );

    // SET method for configuration
    _apiServer.registerMethod("wifi/sta/config",
        APIMethodBuilder(APIMethodType::SET, [this](const JsonObject* args, JsonObject& response) {
            bool success = _wifiManager.setSTAConfigFromJson(*args);
            response["success"] = success;
            return true;
        })
        .desc("Configure Station mode")
        .param("enabled", "bool")
        .param("ssid", "string")
        .param("password", "string")
        .param("dhcp", "bool")
        .response("success", "bool")
        .build()
    );

    // Event for state changes
    _apiServer.registerMethod("wifi/events",
        APIMethodBuilder(APIMethodType::EVT)
        .desc("WiFi status updates")
        .response("status", {
            {"connected", "bool"},
            {"rssi", "int"}
        })
        .build()
    );
}
```

### 3. Event Handling

```cpp
// In WiFiManagerAPI.h
void WiFiManagerAPI::sendNotification() {
    StaticJsonDocument<512> doc;
    JsonObject status = doc.to<JsonObject>();
    _wifiManager.getStatusToJson(status);
    
    _apiServer.broadcast("wifi/events", status);
}
```

### HTTP Usage Example

**GET** `/api/wifi/status`

```json
{
    "ap": {
        "enabled": true,
        "connected": true,
        "clients": 1
    },
    "sta": {
        "enabled": true,
        "connected": true,
        "rssi": -65
    }
}
```

### WebSocket Usage Example

Event received on `/ws`

```json
{
    "event": "wifi/events",
    "data": {
        "status": {
            "connected": true,
            "rssi": -65
        }
    }
}
```

## Best Practices

### Resource Structure

- Use lowercase paths with hyphens
- Group related resources (`wifi/status`, `wifi/config`)
- Use nouns instead of verbs

### API Design

- Separate business logic from API layer
- Use consistent response structures
- Implement proper error handling
- Document all methods and parameters

> **Important:** Always validate input parameters and handle errors appropriately.

## Conclusion

APIServer provides a solid foundation for building APIs on embedded systems.  
Its protocol-agnostic design and clear separation of concerns make it ideal for IoT applications requiring multiple communication protocols.