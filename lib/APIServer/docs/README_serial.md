# Serial API Protocol

## Overview
The Serial API protocol provides a simple, human-readable interface for interacting with the API server through a serial connection (UART or USB CDC). It uses a text-based format with line-oriented commands and responses.

## Message Format

### Commands (client → server)
```
> METHOD path[:param1=value1,param2=value2,...]
```
- `>` : Mandatory prompt character (commands without '>' prefix are ignored)
- `METHOD` : `GET`, `SET`, or `LIST`
- `path` : API endpoint path
- `:` : Separator for parameters (optional for GET)
- Parameters are key-value pairs separated by commas

### Responses (server → client)
```
< METHOD path: param1=value1,param2=value2,...
```
- `<` : Response indicator
- `METHOD` : Echo of the original command method
- `path` : Echo of the API endpoint path
- `:` : Separator for response data
- Response data uses the same key-value format as commands

### Error Responses
```
< METHOD path: error=error_type
```
Error types:
- `invalid command`: Malformed command or timeout
- `command too long`: Command exceeds maximum length (512 bytes)
- `wrong request or parameters`: Invalid API path or parameters
- `command timeout`: Serial read timeout

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
< SET wifi/sta/config: success=true
```

## API Documentation
The `GET api` command returns a complete description of available endpoints:
```
> GET api
< GET api
api.methods:
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
< GET wifi/status: ap.enabled=true,ap.connected=false,ap.clients=0
```

### SET Request
```
> SET wifi/ap/config: enabled=true,ssid=MyAP,password=12345678
< SET wifi/ap/config: success=true
```

### Event Notification
```
< EVT wifi/events: data.status.connected=true,data.ip=192.168.1.100
```

### Error Examples
```
> (invalid command...)
< ERROR: error=invalid command

> GET wifi/invalid/path
< GET wifi/invalid/path: error=wrong request or parameters

> (very long command...)
< ERROR: error=command too long

> (timeout occurs...)
< ERROR: error=command timeout
```

## Implementation Notes

### Features
- Line-oriented protocol (commands end with newline)
- Human-readable format
- Support for nested objects via dot notation
- Automatic parameter conversion (bool, int, float, string)
- Event queue for asynchronous notifications
- Pretty-printed API documentation
- Robust command parsing with timeout detection

### Limitations
- Maximum command length: 512 bytes
- Queue size for events: 10 messages
- Event polling interval: 50ms
- Command timeout: 200ms
- Serial must be initialized before endpoint (typically in setup())
- Commands must start with '>' to be processed

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