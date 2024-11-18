# WiFiManager Library Documentation

## Table of Contents
- [Overview](#overview)
- [Architecture](#architecture)
- [Quick Start](#quick-start)
- [Core Components](#core-components)
- [Configuration](#configuration)
- [Status Management](#status-management)
- [Event System](#event-system)
- [Network Scanning](#network-scanning)
- [Implementation Details](#implementation-details)
- [Important Notes](#important-notes)

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

## Configuration

### Access Point (AP) Configuration

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

#### AP Configuration Parameters
- `enabled (bool)`: Enable/disable AP mode
- `ssid (string)`: Network name (32 chars max)
- `password (string)`: Network password (8-64 chars)
- `channel (int)`: WiFi channel (1-13)
- `ip (string)`: AP IP address
- `gateway (string)`: Gateway IP
- `subnet (string)`: Subnet mask

### Station (STA) Configuration

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

#### STA Configuration Parameters
- `enabled (bool)`: Enable/disable STA mode
- `ssid (string)`: Network to connect to
- `password (string)`: Network password
- `dhcp (bool)`: Use DHCP or static IP
- `ip (string, optional)`: Static IP address
- `gateway (string, optional)`: Gateway IP
- `subnet (string, optional)`: Subnet mask

## Status Management

### Getting Current Status

```cpp
StaticJsonDocument<512> status;
JsonObject statusObj = status.to<JsonObject>();
wifiManager.getStatusToJson(statusObj);
```

Status JSON format:
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

## Network Scanning

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

The WiFiManager library seamlessly integrates with the APIServer library to provide a RESTful API interface for WiFi management. This integration is handled through the `WiFiManagerAPI` class, which acts as a bridge between the WiFiManager's business logic and the API layer.

### Core Components

#### WiFiManagerAPI Class
- Handles API method registration
- Manages state change notifications
- Provides JSON-based API interface
- Coordinates event broadcasting

### API Methods

#### GET Methods
- `wifi/status`: Get current WiFi status
- `wifi/config`: Get current configuration
- `wifi/scan`: Scan available networks

#### SET Methods
- `wifi/ap/config`: Configure Access Point
- `wifi/sta/config`: Configure Station mode
- `wifi/hostname`: Set device hostname

#### EVT Methods
- `wifi/events`: Real-time status and configuration updates

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
        "ip": "",
        "gateway": "",
        "subnet": ""
    }
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

## Important Notes

- Always call `poll()` in your main loop or in a task to maintain the WiFi connection
- Password length must be between 8 and 64 characters
- SSID length must not exceed 32 characters
- Static IP configuration requires valid IP, gateway, and subnet mask
- Configuration is automatically saved to SPIFFS upon changes