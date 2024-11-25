# WiFiManager Library Documentation

## Table of Contents
- [Overview](#overview)
- [Architecture](#architecture)
- [Quick Start](#quick-start)
- [APIServer Integration](#apiserver-integration)
- [Web UI](#web-ui)
- [Implementation Details](#implementation-details)

## Overview

The WiFiManager library provides a comprehensive solution for managing WiFi connections on ESP32 devices. It offers a robust implementation supporting both Access Point (AP) and Station (STA) modes simultaneously.

### Key Features
- Dual mode operation (AP + STA)
- JSON configuration interface
- Automatic reconnection
- Asynchronous operation
- mDNS support
- RESTful API
- Real-time monitoring
- SPIFFS configuration storage

## Architecture

### Core Components

#### WiFiManager
- Manages WiFi functionality (AP/STA modes)
- Handles configurations and states
- Provides JSON interface
- Manages event notifications

#### ConnectionConfig
- Stores WiFi configuration
- Handles JSON serialization
- Validates parameters

#### ConnectionStatus
- Tracks real-time status
- Monitors connections
- Provides JSON output

#### WiFiManagerAPI
- Integrates with APIServer
- Handles API requests
- Manages WebSocket events
- Serves web interface

## Quick Start

The setup is pretty straightforward, just include the WiFiManager header and create an instance of the WiFiManager class.
You must call `begin()` to initialize the WiFiManager, and then call `poll()` in your main loop or in a task to maintain the WiFi connection.

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

Configuration can be done either using ConnectionConfig structures or the JSON interface. The JSON interface provides a more flexible and dynamic way to configure the WiFiManager, as it allows for partial updates and follows the "robustness principle": it automatically fills missing parameters with relevant values and cleans up inapplicable ones.

The simplicity lies in that **you only need to specify the parameters you want to change**. For example, connecting or disconnecting from a WiFi network only requires sending a JSON with an "enabled" key.

> **Note:**  
> - Configuration is saved to SPIFFS upon changes
> - The configuration methods provide both type- and value-checking
> - Invalid configurations are rejected and return false

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

In AP mode, subnet is always 255.255.255.0. Gateway will default to IP if not specified.

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

In Station mode, if DHCP is disabled:
- IP is mandatory
- Gateway and subnet must be specified together
- All parameters are validated for format and consistency

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
        "status": {
            "ap": {...},
            "sta": {...}
        },
        "config": {
            "ap": {...},
            "sta": {...}
        }
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

## Web UI

### Overview
The library includes a web interface for WiFi configuration, providing:
- AP/STA configuration
- Network scanning
- IP settings
- Real-time monitoring
- Hostname configuration

### Accessing the Interface
Available at `http://<device-ip>/` or `http://<hostname>.local/`

> **Important:**  
> Keep at least one connection method active to maintain access.

### Features
- Real-time WebSocket updates
- Network scanning
- Protected configuration
- IP configuration
- Signal monitoring
- Client tracking

### Installation
The web interface files must be uploaded to the device's SPIFFS file system. Using PlatformIO:

1. Place files in `data/` directory
2. Build filesystem: `pio run --target buildfs`
3. Upload: `pio run --target uploadfs`

> **Note:**  
> The web interface is automatically enabled when using the WebAPIEndpoint class. No additional configuration is required beyond uploading the file system.

## Implementation Details

### Memory Management
- Static allocation for JSON documents
- Optimized buffer sizes:
  - Configuration: 1024 bytes
  - Status updates: 1024 bytes
  - Network scan: 1024 bytes (10 networks max)

### Connection Management
- Automatic reconnection with exponential backoff
- Configurable poll intervals and timeouts
- Robust state machine handling
- Connection attempt monitoring

### Configuration
- SPIFFS-based persistent storage
- Automatic loading on startup
- Fallback to safe defaults
- Full parameter validation

### Error Handling
- Configuration validation
- Connection attempt monitoring
- Timeout handling
- State consistency checks
- JSON parsing error detection

## Requirements

> **Important:**  
> This library requires C++17 or higher. Add to `platformio.ini`:
> ```ini
> build_flags = 
>     -Wno-deprecated-declarations
>     -std=gnu++17
> build_unflags = -std=gnu++11
> ```