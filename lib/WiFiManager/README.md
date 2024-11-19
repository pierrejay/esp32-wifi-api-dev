# WiFiManager Library Documentation

## Table of Contents
- [Overview](#overview)
- [Architecture](#architecture)
- [Quick Start](#quick-start)
- [APIServer Integration](#apiserver-integration)
- [Implementation Details](#implementation-details)

## Overview

The WiFiManager library provides a comprehensive solution for managing WiFi connections on ESP32 devices. It offers a robust implementation supporting both Access Point (AP) and Station (STA) modes simultaneously.

### Key Features
- Dual mode operation (AP + STA)
- Configuration persistence in SPIFFS
- JSON-based configuration interface
- Automatic reconnection handling
- mDNS support for local network discovery
- RESTful API integration
- Real-time status monitoring
- Event-driven architecture

## Architecture

### Core Components

#### WiFiManager (Master Class)
- Central manager for all WiFi functionality
- Handles both AP and STA modes
- Manages configurations and states
- Provides JSON interface for operations
- Coordinates event notifications

#### ConnectionConfig (Structure)
- Configuration container for WiFi modes
- Handles serialization/deserialization
- Validates configuration parameters
- Supports both AP and STA specific settings

#### ConnectionStatus (Structure)
- Real-time status information
- Contains connection state details
- Tracks clients, IP, and signal strength
- Provides JSON serialization

#### WiFiManagerAPI (Interface Class)
- Bridges WiFiManager with APIServer library
- Registers WiFi management API methods
- Handles incoming API requests to WiFiManager
- Broadcasts events to the API Server

## Quick Start

The setup is pretty straightforward, just include the WiFiManager header and create an instance of the WiFiManager class.
You must call `begin()` to initialize the WiFiManager, and then call `poll()` in your main loop or in a task to maintain the WiFi connection (update status, handle reconnections, etc.).

```cpp
#include "WiFiManager.h"

WiFiManager wifiManager;

void setup() {
    Serial.begin(115200);
    
    if (!wifiManager.begin()) {
        Serial.println("WiFiManager initialization failed!");
        return;
    }
}

void loop() {
    wifiManager.poll();
}
```

### Configuration

Configuration can be done either by using the provided methods with ConnectionConfig structures, or by using the JSON interface. 
The JSON interface provides a more flexible and dynamic way to configure the WiFiManager, as it allows for a more complex configuration to be loaded from a file or a web server.
Configuration is saved to SPIFFS upon changes, so it will persist across reboots.
The setAPConfig and setSTAConfig methods all provide type- and value-checking of the configuration parameters. Invalid configurations are rejected and return false. It will also clean up configuration items that are not applicable (like channel for AP) by removing them from the configuration before saving.

#### Access Point (AP) Configuration

```cpp
// Using JSON configuration
StaticJsonDocument<512> apConfig;
apConfig["enabled"] = true;
apConfig["ssid"] = "ESP32-AP";
apConfig["password"] = "password123";
apConfig["channel"] = 1;
apConfig["ip"] = "192.168.4.1";
apConfig["gateway"] = "192.168.4.1";
apConfig["subnet"] = "255.255.255.0";

wifiManager.setAPConfigFromJson(apConfig.as<JsonObject>());
```

##### AP Configuration Parameters
- `enabled (bool)`: Enable/disable AP mode
- `ssid (string)`: Network name (32 chars max)
- `password (string)`: Network password (8-64 chars)
- `channel (int)`: WiFi channel (1-13)
- `ip (string)`: AP IP address
- `gateway (string, optional)`: Gateway IP

In Access Point mode, the subnet is always set to 255.255.255.0. IP is mandatory, but gateway will be set to IP if not specified. 

#### Station (STA) Configuration

```cpp
// Using JSON configuration
StaticJsonDocument<512> staConfig;
staConfig["enabled"] = true;
staConfig["ssid"] = "MyNetwork";
staConfig["password"] = "password123";
staConfig["dhcp"] = false;
staConfig["ip"] = "192.168.1.200";
staConfig["gateway"] = "192.168.1.1";
staConfig["subnet"] = "255.255.255.0";

wifiManager.setSTAConfigFromJson(staConfig.as<JsonObject>());
```

##### STA Configuration Parameters
- `enabled (bool)`: Enable/disable STA mode
- `ssid (string)`: Network to connect to
- `password (string)`: Network password
- `dhcp (bool)`: Use DHCP or static IP
- `ip (string, optional)`: Static IP address
- `gateway (string, optional)`: Gateway IP
- `subnet (string, optional)`: Subnet mask

In Station mode, `dhcp` decides if IP, gateway and subnet are used: 
- If DHCP is enabled, IP, gateway and subnet are ignored. 
- If DHCP is disabled, IP, gateway and subnet are used.
  - IP is mandatory, but gateway and subnet are optional.
  - If gateway is specified, then subnet is mandatory, and vice versa.

### Network Scanning

The WiFiManager provides a method to scan for available networks and return the results in a JSON format.

```cpp
StaticJsonDocument<512> networks;
JsonObject networksObj = networks.to<JsonObject>();
wifiManager.getAvailableNetworks(networksObj);
```

Scan results format:
```json
{
    "networks": [
        {
            "ssid": "Network1",
            "rssi": -65,
            "encryption": 3
        }
    ]
}
```

## APIServer Integration

### Overview

The WiFiManager library seamlessly integrates with the APIServer library to provide a RESTful API interface for WiFi management & WebSocket notifications. This integration is handled through the `WiFiManagerAPI` class, which acts as a bridge between the WiFiManager's business logic and the API layer.
The HTTP & WebSocket endpoints are provided by the `ESPAsyncWebServer` library, which is integrated with the APIServer through a dedicated `WebAPIEndpoint` integration class.
All REST methods are automatically documented and available at the `/api` path. 
The operation of the HTTP/WebSocket server is fully asynchronous and non-blocking, and does not interfere with the WiFiManager's operation.

### Core Components

#### WiFiManagerAPI Class
- Handles API method registration
- Manages state change notifications
- Provides JSON-based API interface
- Coordinates event broadcasting

### API Methods

#### GET Methods : `HTTP GET`
- `api/wifi/status`: Get current WiFi status
- `api/wifi/config`: Get current configuration
- `api/wifi/scan`: Scan available networks

#### SET Methods : `HTTP POST`
- `api/wifi/ap/config`: Configure Access Point
- `api/wifi/sta/config`: Configure Station mode
- `api/wifi/hostname`: Set device hostname

#### EVT Methods : `WebSocket`
- `api/events`: Real-time status and configuration updates

### API Responses

#### Status Response Format
```json
{
    "ap": {
        "enabled": true,
        "connected": true,
        "clients": 2,
        "ip": "192.168.4.1",
        "rssi": 0
    },
    "sta": {
        "enabled": true,
        "connected": true,
        "ip": "192.168.1.200",
        "rssi": -65
    }
}
```

#### Configuration Response Format
```json
{
    "ap": {
        "enabled": true,
        "ssid": "ESP32-AP",
        "password": "********",
        "channel": 1,
        "ip": "192.168.4.1",
        "gateway": "192.168.4.1",
        "subnet": "255.255.255.0"
    },
    "sta": {
        "enabled": true,
        "ssid": "HomeNetwork",
        "password": "********",
        "dhcp": true,
        "ip": "192.168.1.200",
        "gateway": "192.168.1.1",
        "subnet": "255.255.255.0"
    }
}
```

#### Event Format
An event message is formed by the former `status` and a `config` objects concatenated in a single `data` object.
Events will be notified following this layout:
```json
{
    "event": "wifi/events",
    "data": {
        "status": {...},
        "config": {...}
    }
}
```

### Operations
By default, the WiFiManager automatically registers the API methods and events when instantiated, and then serves the incoming `GET` & `POST` requests from the API Server.
It will also send automatic updates to WebSocket clients when the WiFi connection state changes & regular status updates.
This will allow for an asynchronous and non-blocking operation of the WiFiManager, without interfering with the API Server :
- When a client sends a `POST` request (for example to connect to a WiFi network), the WiFiManagerAPI will set the value and send a success response back immediately. This success response only indicates that the request is valid and being processed, but does not indicate that the task is completed.
- The client should then rely on the `WebSocket` events to be notified when the connection is established or an error occurs.
- `GET` requests are served immediately, since they do not modify the state

### Implementation Example

```cpp
#include "WiFiManager.h"
#include "WiFiManagerAPI.h"
#include "APIServer.h"
#include "WebAPIEndpoint.h"

WiFiManager wifiManager;
APIServer apiServer;
WiFiManagerAPI wifiManagerAPI(wifiManager, apiServer);
WebAPIEndpoint webServer(apiServer, 80);

void setup() {
    apiServer.addEndpoint(&webServer);
    
    if (!wifiManager.begin()) {
        Serial.println("WiFiManager initialization error");
        while(1) delay(1000);
    }
    
    apiServer.begin();
}

void loop() {
    wifiManager.poll();     // Poll WiFiManager
    wifiManagerAPI.poll();  // Poll API interface
    apiServer.poll();       // Poll API server
}
```


### Event System Integration

The WiFiManagerAPI automatically broadcasts state changes through the APIServer's event system:

```cpp
void onStateChange() {
    StaticJsonDocument<2048> newState;
    JsonObject status = newState["status"].to<JsonObject>();
    JsonObject config = newState["config"].to<JsonObject>();
    
    // Get current state
    wifiManager.getStatusToJson(status);
    wifiManager.getConfigToJson(config);
    
    // Broadcast update
    apiServer.broadcast("wifi/events", newState.as<JsonObject>());
}
```

### Implementation Notes

- State changes are broadcast with a minimum interval of 500ms
- Events are automatically sent to all compatible endpoints (WebSocket, MQTT)
- Method parameters are validated before execution
- All responses follow a consistent JSON format
- API endpoints automatically handle parameter validation

## Implementation Details

### Memory Management
- Static allocation for JSON documents
- Default buffer sizes:
  - Configuration: 1024 bytes
  - Status updates: 512 bytes
  - Network scan: 256 bytes per network

### Connection Management
- Automatic reconnection handling
- Configurable poll & retry interval, timeouts

### Configuration Persistence
- Configurations stored in SPIFFS
- Automatic loading on startup
- Default configurations if no stored data

### Validation
- IP address & subnet mask format validation
- SSID and password length checks
- Channel number validation (1-13)
- Configuration completeness checks

### Error Handling
- Configuration validation
- Connection attempt monitoring
- Timeout handling
- State consistency checks
- JSON parsing error detection