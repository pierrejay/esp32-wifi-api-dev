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
- `command too long`: Command exceeds maximum length (4096 bytes)
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

    wifi/status
    ├── type: GET
    ├── desc: Get WiFi status
    ├── protocols: http|serial
    └── response:
        ├── ap.enabled: bool
        ├── ap.connected: bool
        └── ap.clients: int

    wifi/sta/config
    ├── type: SET
    ├── desc: Configure Station mode
    ├── protocols: serial
    ├── params:
    │   ├── enabled: bool
    │   ├── network.ssid: string
    │   └── network.password: string
    └── response:
        └── success: bool
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
- Command parsing with timeout detection

### Serial Port Sharing
The SerialAPIEndpoint implements a transparent proxy mechanism that allows sharing the serial port between API commands and regular application traffic:

- Commands starting with '>' are intercepted and processed by the API
- All other traffic is buffered through two 1KB circular buffers (RX/TX)
- The proxy is automatically installed by defining `Serial` as `SerialAPIEndpoint::proxy`
- Application code can use `Serial` normally without being aware of the API

Example with mixed traffic:
```
Serial.println("Hello World");     // Regular application output
> GET wifi/status                  // API command
< GET wifi/status: ...            // API response
Serial.println("Done");           // Regular application output
```

This allows seamless integration of the Serial API without modifying existing application code that uses the serial port for debugging or other purposes.

Note: The proxy buffer size is limited to 1024 bytes. If your application sends large amounts of data through Serial, you may need to adjust `SerialProxy::BUFFER_SIZE`.

### Limitations
- Maximum command length: 4096 bytes
- Queue size for events: 10 messages
- Serial must be initialized after endpoint (typically in setup())
- Commands must start with '>' to be processed
- Command timeout: 200ms by default
- Event queue polling interval: 100ms by default
- Interval between API & proxy messages: 5ms by default

### Tips
- Make sure the buffer size is big enough to handle the longest command. 
- If necessary, split the command into multiple smaller commands to reduce buffer size as much as possible.

### State Machine
The SerialAPIEndpoint uses a state machine to handle all serial communications efficiently.
The goal is to correctly arbitrate between API and proxy traffic over the access to the Serial port, but also to avoid blocking the thread with a non-blocking approach.

States:
- `NONE`: Idle state, waiting for input or events
- `PROXY_RECEIVE`: Receiving regular serial data
- `PROXY_SEND`: Sending buffered proxy data
- `API_RECEIVE`: Building an API command
- `API_PROCESS`: Processing the command
- `API_RESPOND`: Sending API response
- `EVENT`: Sending event notification

Each state transition is managed with a grace period to ensure reliable communication:
- After sending data (PROXY_SEND, API_RESPOND, EVENT): waits 50ms before accepting new input
- During command reception (API_RECEIVE, PROXY_RECEIVE): times out after 50ms of inactivity
- Between proxy operations: ensures 50ms spacing

To allow large messages to be processed in a non-blocking way, received serial messages are processed by chunks of 128 bytes at each step. Responses and events are sent in full by default to increase the reliability of the communication (no interruption possible).
> **Note:**  
> - The chunk size can be adjusted by changing the `TX_CHUNK_SIZE` & `RX_CHUNK_SIZE` constants. Be careful with these settings, too small values may degrade the performance, too high values may deplete the RX buffer quickly or overflow the TX buffer.
> - The default API write cycle could block the main thread due to serial communication, in particular with long messages (e.g. ~500ms for a 4096 byte message @ 9600 bps). 
> - Optionally, you could set the maximal number of chunks to process at each write cycle by setting the "MAX_TX_CHUNKS" variable. Default value 0 means all chunks will be sent (blocking).

This approach:
- Avoids blocking the thread by returning to the parent loop or task regularly
- Prevents message collisions
- Ensures complete transmission of responses
- Maintains clean separation between API and proxy traffic
- Handles timeouts gracefully with appropriate error messages

Example timing sequence:
```
[NONE] → received '>' → [API_RECEIVE]
[API_RECEIVE] → received complete command → [API_PROCESS]
[API_PROCESS] → command processed → [API_RESPOND]
[API_RESPOND] → response sent → wait 50ms → [NONE]
```

The state machine also manages the event queue, sending events only when the system is idle and respecting the same grace periods to maintain reliable communication.

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