# ESP32 WiFiManager & APIServer Libraries

This project provides a comprehensive solution for managing WiFi connections and implementing extensible communication protocols on ESP32 devices. It consists of two main libraries that work together seamlessly while remaining independently usable.

> ⚠️ **Warning**   
> This repository is currently under development (Work in Progress). Features may be incomplete or subject to change without notice. Use with caution.

## Overview

### [WiFiManager Library](lib/WiFiManager/README.md)
A flexible WiFi connection manager designed for real-world IoT deployments. It provides multiple interfaces to configure and manage ESP32's WiFi connections:
- Direct method calls for programmatic control
- Rich web interface for human interaction
- RESTful API for remote management
- Persistent configuration storage

### [APIServer Library](lib/APIServer/README.md)
A protocol-agnostic API framework built for simplicity and extensibility, separating:
- Core API handling
- Protocol implementations
- Business logic

Architecture highlights:
- Abstract `APIEndpoint` base class
- Builder pattern for method registration
- Polymorphic protocol handling
- Event-driven updates

Supported protocols through endpoint implementations:
- HTTP REST / WebSocket (with `ESPAsyncWebServer`)
- MQTT (with `PubSubClient`)
- Serial (debugging and local control)
- Custom protocols (RF, BLE, ...) through the extensible endpoint system

The library's abstraction layer allows new protocols to be added by simply implementing the `APIEndpoint` interface, without modifying the core API or business logic. This makes it particularly suitable for:
- Multi-protocol IoT systems
- Future-proof products
- Evolving communication needs
- Custom protocol development

## Architecture
Simplified project structure:
```
project/
├── lib/
│   ├── WiFiManager/              # WiFi Management Library
│   │   ├── src/
│   │   │   ├── WiFiManager.cpp
│   │   │   ├── WiFiManager.h
│   │   │   └── WiFiManagerAPI.h
│   │   └── README.md             # WiFiManager Documentation
│   │
│   └── APIServer/                # API Server Library
│       ├── src/
│       │   ├── APIServer.h
│       │   ├── APIEndpoint.h
│       │   └── WebAPIEndpoint.h
│       └── README.md             # APIServer Documentation
│
├── src/
│   └── main.cpp                  # Main application
│
└── README.md                     # This file
```

## Key Features

### Integration
- The libraries are designed to work together seamlessly but can be used independently
- WiFiManagerAPI class serves as a bridge between WiFiManager and APIServer
- Modular architecture allows easy addition of new features and protocols

### Real-time Operations
- Non-blocking asynchronous operations
- Event-driven status updates
- WebSocket support for real-time communication
- Automatic state management

### Configuration
- JSON-based configuration interface
- SPIFFS-based persistence
- Web UI for easy setup
- Comprehensive parameter validation

## Quick Start

1. Include the required headers:
```cpp
#include "WiFiManager.h"
#include "WiFiManagerAPI.h"
#include "APIServer.h"
#include "WebAPIEndpoint.h"
```

2. Create the necessary objects:
```cpp
WiFiManager wifiManager;
APIServer apiServer;
WiFiManagerAPI wifiManagerAPI(wifiManager, apiServer);
WebAPIEndpoint webServer(apiServer, 80);
```

3. Initialize the system:
```cpp
void setup() {
    apiServer.addEndpoint(&webServer);
    
    if (!wifiManager.begin()) {
        Serial.println("WiFiManager initialization error");
        return;
    }
    
    apiServer.begin();
}
```

4. Poll the components in your main loop:
```cpp
void loop() {
    wifiManager.poll();     // Poll WiFiManager
    wifiManagerAPI.poll();  // Poll API interface
    apiServer.poll();       // Poll API server
}
```

## Web Interface

The system provides a built-in web interface accessible at:
- `http://<device-ip>/` when connected to the device's network
- `http://<hostname>.local/` when mDNS is supported

### Features
- WiFi network configuration
- Access Point settings
- Real-time status monitoring
- Network scanning
- Device configuration

## API Documentation

The complete API documentation is automatically generated and available at:
- `http://<device-ip>/api` or
- `http://<hostname>.local/api`

## Library Documentation

For detailed information about each library:

- [WiFiManager Documentation](lib/WiFiManager/README.md)
  - WiFi connection management
  - Configuration options
  - Status monitoring
  - Web interface details

- [APIServer Documentation](lib/APIServer/README.md)
  - API implementation
  - Protocol support
  - Event system
  - Custom endpoint creation