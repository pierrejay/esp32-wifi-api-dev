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

#### 1. Static Allocation (default)
```cpp
// Fixed buffer sizes for different types of responses
static constexpr size_t GET_JSON_BUF = 2048;   // GET responses
static constexpr size_t SET_JSON_BUF = 512;    // SET responses
static constexpr size_t DOC_JSON_BUF = 4096;   // API documentation
static constexpr size_t WS_JSON_BUF = 1024;    // WebSocket events

// Example of static allocation
StaticJsonDocument<GET_JSON_BUF> doc;
JsonObject response = doc.to<JsonObject>();
// ... fill response ...
char buffer[GET_JSON_BUF];
serializeJson(doc, buffer);
request->send(200, MIME_JSON, buffer);
```

**Pros:**
- Predictable memory usage
- No heap fragmentation from JSON documents
- Safer for long-running applications

**Cons:**
- String values are still allocated on heap (ArduinoJson limitation)
- Fixed buffer sizes might be wasteful for small responses
- Need to handle buffer overflow carefully
- Internal queues and debug logs still use dynamic allocation
- Not truly "static" as the name suggests

> **Implementation Detail:**
> Even in "static" mode, some dynamic allocations still occur:
> - WebSocket event queue (std::queue<String>)
> - Debug/startup logs (std::vector<String>)
> 
> For a truly static implementation, you would need to:
> - Use fixed-size circular buffers instead of queues
> - Pre-allocate string buffers for logs
> - Disable debug features
> - Use char arrays instead of String
> 
> However, this would make the code much more complex and rigid.

#### 2. Dynamic Allocation (optional)
Enable with `#define USE_DYNAMIC_JSON_ALLOC`
```cpp
// Create dynamic response with specified capacity
AsyncJsonResponse* response = new AsyncJsonResponse(false, GET_JSON_BUF);
JsonObject root = response->getRoot();
// ... fill response ...
response->setLength();
request->send(response);
```

**Pros:**
- More memory efficient for varying response sizes
- Built-in integration with ESPAsyncWebServer
- Simpler response handling code

**Cons:**
- Can lead to heap fragmentation
- Need to monitor heap usage carefully
- Risk of memory allocation failures under load

> **Implementation Note:**  
> The choice between static and dynamic allocation depends on your use case:
> - Use static allocation for predictable, small responses
> - Consider dynamic allocation if:
>   - Response sizes vary greatly
>   - Memory is tight and fixed buffers would be wasteful
>   - You need to handle many simultaneous requests
> - Monitor heap usage and fragmentation in both cases
> - Consider implementing a response pool for high-traffic scenarios

> **Memory Safety:**  
> Neither approach is perfectly safe by default:
> - Static allocation still uses heap for strings
> - Dynamic allocation can fail under memory pressure
> - Consider implementing:
>   - Memory monitoring
>   - Request rate limiting
>   - Response size limits
>   - Error handling for allocation failures

A third approach could be implemented for maximum safety:
- Pre-allocated response templates
- Fixed-size string buffers
- No dynamic allocations
Though this will increase the complexity of the code and reduce flexibility, it will be a focus for future releases in order to ensure maximum safety in production environments.

> **TLDR:**  
> For most scenarios, the default dynamic allocation approach is fully safe and more than enough.

### Limitations
- WebSocket events queue size: 10 messages
- WebSocket polling interval: 50ms
- WebSocket API disabled by default (`WS_API_ENABLED = false`)

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

## Best Practices
1. Keep JSON payloads small and well-structured
2. Use HTTP for configuration and state changes
3. Use WebSocket only for real-time events
4. Consider enabling CORS for web development
5. Add proper error handling in the web interface
6. Monitor WebSocket connection state