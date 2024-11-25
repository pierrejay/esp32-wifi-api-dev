# Web API Protocol

## Overview
The Web API provides two complementary protocols:
- HTTP REST API for GET and SET operations
- WebSocket for real-time events notifications

## HTTP REST API

### Endpoint Structure
```
http://<device-ip>/api/<path>
```

### GET Requests
```http
GET /api/wifi/status HTTP/1.1
```
Response:
```json
{
  "ap": {
    "enabled": true,
    "connected": false,
    "clients": 0,
    "ip": "192.168.4.1",
    "rssi": -70
  },
  "sta": {
    "enabled": true,
    "connected": true,
    "ip": "192.168.1.100",
    "rssi": -65
  }
}
```

### SET Requests
```http
POST /api/wifi/sta/config HTTP/1.1
Content-Type: application/json

{
  "enabled": true,
  "network": {
    "ssid": "MyWiFi",
    "password": "12345678",
    "security": {
      "type": "WPA2-EAP",
      "certificates": {
        "ca": "-----BEGIN CERTIFICATE-----...",
        "client": "-----BEGIN CERTIFICATE-----..."
      }
    }
  }
}
```
Response:
```json
{
  "success": true
}
```

### API Documentation
Available at `/api`:
```http
GET /api HTTP/1.1
```
Returns a JSON description of all available methods:
```json
{
  "methods": [{
    "path": "wifi/status",
    "type": "GET",
    "desc": "Get WiFi status",
    "protocols": ["http", "websocket"],
    "response": {
      "ap": {
        "enabled": "bool",
        "connected": "bool",
        "clients": "int",
        "ip": "string",
        "rssi": "int"
      }
    }
  },
  ...
  ]
}
```

### Error Handling
Errors follow standard HTTP status codes:
- 200: Success
- 400: Bad Request (invalid parameters)
- 404: Not Found (invalid endpoint)

Error responses contain a JSON error message:
```json
{
  "error": "Bad Request"
}
```

## WebSocket Events

### Connection
Connect to the WebSocket endpoint:
```
ws://<device-ip>/api/events
```

### Event Format
Events are pushed from server to client as JSON messages:
```json
{
  "event": "wifi/events",
  "data": {
    "status": {
      "connected": true,
      "ip": "192.168.1.100"
    }
  }
}
```

## Implementation Notes

### Features
- Asynchronous HTTP server (ESPAsyncWebServer)
- JSON-based REST API
- Static file serving from SPIFFS (web interface)
- WebSocket for real-time events
- Automatic API documentation endpoint
- No authentication (designed for local network use)

### Memory Management

Two approaches are available for handling JSON responses:

#### 1. Static JSON Buffers (default)
```cpp
// Fixed buffer sizes for different types of responses
static constexpr size_t GET_JSON_BUF = 2048;   // GET responses
static constexpr size_t SET_JSON_BUF = 512;    // SET responses
static constexpr size_t DOC_JSON_BUF = 4096;   // API documentation
static constexpr size_t WS_JSON_BUF = 1024;    // WebSocket events

// Example usage
StaticJsonDocument<GET_JSON_BUF> doc;
JsonObject response = doc.to<JsonObject>();
// ... fill response ...
char buffer[GET_JSON_BUF];
serializeJson(doc, buffer);
request->send(200, MIME_JSON, buffer);
```

#### 2. Dynamic Allocation (optional)
Enable with `#define USE_DYNAMIC_JSON_ALLOC`
```cpp
AsyncJsonResponse* response = new AsyncJsonResponse(false, GET_JSON_BUF);
JsonObject root = response->getRoot();
// ... fill response ...
response->setLength();
request->send(response);
```

> **Note on Memory Safety:**  
> - Both approaches still use heap allocation for strings and internal queues
> - Dynamic allocation is the recommended approach and is safe for most use cases
> - For critical applications requiring zero dynamic allocation, a more rigid implementation will be available in future releases
> - Consider implementing request rate limiting and monitoring heap usage in production

### Limitations
- HTTP requests size is limited to 4096 bytes by default (configurable with `MAX_REQUEST_SIZE`), acting as a basic "DoS firewall"
- For REST API, only GET and POST methods are supported but this may be extended if necessary
- WebSocket events queue size is limited to 10 messages (w/o individual message size limit)
- WebSocket queue processing interval is 50ms by default (configurable with `WS_POLL_INTERVAL`)
- WebSocket GET/SET API is disabled by default (`WS_API_ENABLED = false`), only events are supported

### Static File Serving
- Root directory: `/`
- Default file: `index.html`
- Files must be uploaded to SPIFFS

### Usage Example
```cpp
APIServer apiServer;
WebAPIEndpoint webEndpoint(apiServer, 80);  // HTTP port 80

void setup() {
    // Initialize SPIFFS for static files
    if(!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    // Start the web server
    apiServer.addEndpoint(&webEndpoint);
    apiServer.begin();
}

void loop() {
    apiServer.poll();  // Process WebSocket events queue
}
```

### MIME Types
- JSON responses: `application/json`
- Plain text: `text/plain`
- Static files: auto-detected by extension

### Constants
```cpp
static constexpr const char* API_ROUTE = "/api";
static constexpr const char* WS_ROUTE = "/api/events";
static constexpr unsigned long WS_POLL_INTERVAL = 50;
static constexpr size_t WS_QUEUE_SIZE = 10;
static constexpr bool WS_API_ENABLED = false;
```

### Dependencies
- ESPAsyncWebServer
- AsyncWebSocket
- AsyncJson
- SPIFFS
- ArduinoJson