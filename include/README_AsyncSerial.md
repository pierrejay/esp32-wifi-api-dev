# AsyncSerial Library

Thread-safe asynchronous serial communication library enabling multiple logical streams over a single physical UART, with support for both non-blocking and synchronous operations.

## Core Concept: Serial Proxies

### Overview
Serial Proxies are virtual serial ports that share a single physical UART. Each proxy:
- Inherits from Arduino's `Stream` class
- Provides the same API as the standard `Serial` object
- Can be used as a drop-in replacement in existing code

```cpp
// Existing code using Serial
Serial.println("Debug message");
while(Serial.available()) {
    char c = Serial.read();
}

// Same code using a proxy - 100% compatible
SerialProxy debug;
debug.println("Debug message");
while(debug.available()) {
    char c = debug.read();
}
```

### Creating Proxies
Proxies can be configured for different use cases:

```cpp
// Debug logging proxy
SerialProxy debug({
    .rxBufferSize = 1024,    // Receive buffer
    .txBufferSize = 1024,    // Transmit buffer
    .mode = TransmitMode::BEST_EFFORT,  // Non-blocking
    .interMessageDelay = 5   // ms between messages
});

// Protocol proxy (e.g., Modbus)
SerialProxy modbus({
    .mode = TransmitMode::SYNCHRONOUS,  // Request-response
    .rxRequestTimeout = 100,  // Time to respond
    .txResponseTimeout = 50,  // Time to receive response
    .chunkSize = 0           // Send complete messages
});
```

### Multiple Independent Streams
Each proxy operates independently:
```cpp
// Different components can use their own proxy
class Logger {
    SerialProxy _debug;
public:
    void log(const char* msg) { _debug.println(msg); }
};

class ModbusDevice {
    SerialProxy _serial;
public:
    void sendRequest() { 
        _serial.write(request);  // Synchronous mode
        _serial.readBytes(response, len); 
    }
};

// All communications are multiplexed automatically
logger.log("Starting request...");  // Non-blocking
modbus.sendRequest();              // Blocks for response
logger.log("Request complete");    // Waits for modbus
```

### Transparent Integration
Existing libraries can be adapted easily:
```cpp
// Original library using Serial
class ExistingLib {
    Stream& _serial;
public:
    ExistingLib(Stream& serial = Serial) : _serial(serial) {}
    void process() { _serial.println("Working..."); }
};

// Use with any proxy
SerialProxy debug;
ExistingLib lib(debug);  // Works transparently
```

## Architecture Benefits

### 1. Isolation
- Each component gets its own virtual serial port
- No interference between different protocols
- Independent buffer sizes and configurations

### 2. Priority Management
- Critical protocols get guaranteed response windows
- Debug output doesn't interfere with timing
- Automatic handling of transmission priorities

### 3. Resource Optimization
- Single hardware UART
- Shared bandwidth
- Efficient memory usage through configurable buffers

### 4. Thread Safety
- Safe concurrent access from multiple tasks
- Cooperative multitasking through polling
- Atomic operations for synchronization

## Real-World Example

```cpp
// Debug proxy for logging
SerialProxy debug({
    .mode = TransmitMode::BEST_EFFORT,
    .txBufferSize = 2048  // Large buffer for logs
});

// Modbus master proxy
SerialProxy modbus({
    .mode = TransmitMode::SYNCHRONOUS,
    .rxRequestTimeout = 100,
    .txResponseTimeout = 50,
    .txBufferSize = 256   // Standard Modbus frame
});

// AT command proxy
SerialProxy at({
    .mode = TransmitMode::SYNCHRONOUS,
    .rxRequestTimeout = 1000,  // Longer timeout for AT
    .txBufferSize = 512
});

void loop() {
    // All these operations are automatically coordinated
    debug.println("System status: OK");  // Non-blocking
    
    modbus.write(request);              // Waits for response
    while(modbus.available()) {
        processModbusResponse();
    }
    
    at.println("AT+STATUS");            // Waits for response
    String response = at.readString();
    
    AsyncSerial::getInstance().poll();   // Keep the system running
}
```

## Core Components

### State Machine
The library uses a simple state machine to manage serial operations:
- **IDLE**: Looking for work (read or write)
- **READ**: Processing incoming data
- **WRITE**: Sending data from proxies
- **FLUSH**: Handling blocking flush requests

This design ensures:
- No race conditions between operations
- Clear priority handling
- Predictable behavior

### Thread Safety
Thread safety is achieved without mutexes through:

1. **Atomic Flush Operation**
```cpp
std::atomic<SerialProxy*> _flushingProxy{nullptr};
void flush(SerialProxy* proxy) {
    while(!_flushingProxy.compare_exchange_weak(expected, proxy)) {
        poll();  // Cooperative waiting
    }
}
```

2. **Cooperative Multitasking**
- All threads contribute to processing through `poll()`
- Waiting threads help advance the state machine
- Natural FIFO ordering of operations

3. **Priority Management**
- Synchronous proxies get reserved time slots
- Response windows are guaranteed
- Non-critical operations wait for high-priority exchanges

### Communication Modes

#### BEST_EFFORT Mode
- Non-blocking operation
- Immediate transmission attempt
- Ideal for logs, debug output
- No guaranteed delivery timing

```cpp
SerialProxyConfig debugConfig{
    .mode = TransmitMode::BEST_EFFORT,
    .interMessageDelay = 5
};
```

#### SYNCHRONOUS Mode
- Request-response pattern
- Configurable timeouts:
  - `txResponseTimeout`: How long to wait for response
  - `rxRequestTimeout`: Time reserved for responding
- Guaranteed response windows
- Perfect for protocols like Modbus

```cpp
SerialProxyConfig modbusConfig{
    .mode = TransmitMode::SYNCHRONOUS,
    .rxRequestTimeout = 100,  // 100ms to respond
    .txResponseTimeout = 50   // 50ms to receive response
};
```

### Implementation Details

#### Proxy States
Each proxy maintains:
- Independent RX/TX buffers
- Transmission mode
- Timing configuration
- Current operation state

#### Priority Handling
```cpp
// Give priority to proxies processing requests
if (state.isProcessingRequest && 
    now - state.lastRxTime <= state.config.rxRequestTimeout) {
    _state = State::IDLE;
    return;  // Other proxies must wait
}
```

#### Blocking Operations
The `flush()` operation:
1. Waits for current transmission
2. Forces immediate buffer transmission
3. Returns only when complete
4. Maintains thread safety through atomic operations

## Usage Examples

### Debug Logger
```cpp
SerialProxy debug({
    .mode = TransmitMode::BEST_EFFORT,
    .interMessageDelay = 5
});

debug.println("Log message");  // Non-blocking
debug.flush();                 // Wait for transmission
```

### Protocol Implementation
```cpp
SerialProxy protocol({
    .mode = TransmitMode::SYNCHRONOUS,
    .rxRequestTimeout = 100,
    .txResponseTimeout = 50
});

protocol.write("Request");     // Blocks other proxies
protocol.readString();         // Gets response
```

## Best Practices

1. **Mode Selection**
   - Use BEST_EFFORT for non-critical data
   - Use SYNCHRONOUS for protocols requiring responses

2. **Timeout Configuration**
   - Set realistic response timeouts
   - Consider device processing time
   - Account for transmission delays

3. **Buffer Sizing**
   - Size according to message patterns
   - Consider memory constraints
   - Allow for message bursts

4. **Polling**
   - Call poll() regularly in main loop/task
   - Don't block for extended periods
   - Handle timeouts appropriately
