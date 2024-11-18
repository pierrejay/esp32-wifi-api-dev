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
    
    // Optional: Register for state changes
    wifiManager.onStateChange([]() {
        Serial.println("WiFi state changed");
    });
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
- `enabled` (bool): Enable/disable AP mode
- `ssid` (string): Network name (32 chars max)
- `password` (string): Network password (8-64 chars)
- `channel` (int): WiFi channel (1-13)
- `ip` (string): AP IP address
- `gateway` (string): Gateway IP
- `subnet` (string): Subnet mask

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
- `enabled` (bool): Enable/disable STA mode
- `ssid` (string): Network to connect to
- `password` (string): Network password
- `dhcp` (bool): Use DHCP or static IP
- `ip` (string, optional): Static IP address
- `gateway` (string, optional): Gateway IP
- `subnet` (string, optional): Subnet mask

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

## Event System

### State Change Notifications

```cpp
wifiManager.onStateChange([]() {
    StaticJsonDocument<512> status;
    JsonObject statusObj = status.to<JsonObject>();
    wifiManager.getStatusToJson(statusObj);
    
    // Handle status change
    bool apConnected = statusObj["ap"]["connected"];
    bool staConnected = statusObj["sta"]["connected"];
});
```

## Implementation Details

### Memory Management
- Static allocation for JSON documents
- Default buffer sizes:
  - Configuration: 1024 bytes
  - Status updates: 512 bytes
  - Network scan: 256 bytes per network

### Connection Management
- Automatic reconnection handling
- Configurable timeouts:
  - Connection attempt: 30 seconds
  - Retry interval: 30 seconds
  - Status poll interval: 2 seconds

### Configuration Persistence
- Configurations stored in SPIFFS
- Automatic loading on startup
- Manual save option available
- Default configurations if no stored data

### Constants
```cpp
// Default configuration
DEFAULT_AP_SSID = "ESP32-Access-Point"
DEFAULT_AP_PASSWORD = "12345678"
DEFAULT_HOSTNAME = "esp32"

// Timing constants
POLL_INTERVAL = 2000        // Status check interval (ms)
CONNECTION_TIMEOUT = 30000  // Connection attempt timeout (ms)
RETRY_INTERVAL = 30000      // Time between connection retries (ms)
```

### Validation
- IP address format validation
- Subnet mask validation
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

- Always call `poll()` in your main loop
- Password length must be between 8 and 64 characters
- SSID length must not exceed 32 characters
- Static IP configuration requires valid IP, gateway, and subnet mask
- Configuration is automatically saved to SPIFFS
- The library implements comprehensive validation for all inputs