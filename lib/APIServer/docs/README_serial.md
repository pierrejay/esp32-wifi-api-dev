# Serial API Protocol

## Overview
The Serial API protocol provides a simple, human-readable interface for interacting with the API server through a serial connection (UART or USB CDC). It uses a text-based format with line-oriented commands and responses.

## Message Format

### Commands (client → server)
```
> METHOD path[:param1=value1,param2=value2,...]
```
- `>` : Optional prompt character
- `METHOD` : `GET`, `SET`, or `LIST`
- `path` : API endpoint path
- `:` : Separator for parameters (optional for GET)
- Parameters are key-value pairs separated by commas

### Responses (server → client)
```
< path: param1=value1,param2=value2,...
```
- `<` : Response indicator
- `path` : Echo of the API endpoint path
- `:` : Separator for response data
- Response data uses the same key-value format as commands

### Events (server → client)
```
< EVT event_name: data.param1=value1,data.param2=value2,...
```
- `EVT` : Event indicator
- `event_name` : Name of the event
- Event data follows the same format as responses

## Nested Objects
Nested objects use dot notation:
```
> SET wifi/sta/config: network.ssid=MyWiFi,network.security.type=WPA2
< wifi/sta/config: success=true
```

## API Documentation
The `LIST api` command returns a complete description of available endpoints:
```
> LIST api
< api.methods:
  wifi/status:
    type=GET,
    desc=Get WiFi status,
    protocols=http|serial,
    response.ap.enabled=bool,
    response.ap.connected=bool,
    response.ap.clients=int
    
  wifi/sta/config:
    type=SET,
    desc=Configure Station mode,
    protocols=serial,
    params.enabled=bool,
    params.network.ssid=string,
    params.network.password=string,
    response.success=bool
```

## Examples

### GET Request
```
> GET wifi/status
< wifi/status: ap.enabled=true,ap.connected=false,ap.clients=0
```

### SET Request
```
> SET wifi/ap/config: enabled=true,ssid=MyAP,password=12345678
< wifi/ap/config: success=true
```

### Event Notification
```
< EVT wifi/events: data.status.connected=true,data.ip=192.168.1.100
```

## Error Handling
Errors are indicated with an error message:
```
< error: invalid_command
< error: invalid_request
```

## Implementation Notes

### Features
- Line-oriented protocol (commands end with newline)
- Human-readable format
- Support for nested objects via dot notation
- Automatic parameter conversion (bool, int, float, string)
- Event queue for asynchronous notifications
- Pretty-printed API documentation

### Limitations
- Maximum line length: 512 bytes
- Queue size for events: 10 messages
- Polling interval: 50ms
- Serial must be initialized before endpoint (typically in setup())

### Usage
```cpp
APIServer apiServer;
SerialAPIEndpoint serialEndpoint(apiServer, Serial);  // Works with HardwareSerial or USB CDC

void setup() {
    Serial.begin(115200);
    apiServer.addEndpoint(&serialEndpoint);
    apiServer.begin();
}

void loop() {
    apiServer.poll();  // Process commands and events
}
```