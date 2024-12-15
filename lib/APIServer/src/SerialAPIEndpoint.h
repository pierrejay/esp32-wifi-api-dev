#ifndef SERIALAPIENDPOINT_H
#define SERIALAPIENDPOINT_H

#include <ArduinoJson.h>
#include <queue>
#include "APIServer.h"
#include "APIEndpoint.h"
#include "SerialProxy.h"
#include "SerialAPIFormatter.h"

class SerialAPIEndpoint : public APIEndpoint {
public:
    static SerialProxy proxy;  // Public static proxy

    SerialAPIEndpoint(APIServer& apiServer, Stream& serial = Serial) 
        : APIEndpoint(apiServer)
        , _serial(serial)
        , _lastTxRx(0)
        , _apiBufferIndex(0)
    {
        // Declare supported protocols
        addProtocol("serial", GET | SET | EVT);
    }

    void begin() override {
    }

    void poll() override {
        processStateMachine();      // Machine à états unique qui gère tout
    }

    void pushEvent(const String& event, const JsonObject& data) override {
        if (_eventQueue.size() >= QUEUE_SIZE) {
            _eventQueue.pop();
        }
        _eventQueue.push(SerialAPIFormatter::formatEvent(event, data));
    }

private:
    enum class SerialMode {
        NONE,           // Waiting for client input
        PROXY_RECEIVE,  // Receiving data for the proxy
        PROXY_SEND,     // Sending data to the proxy
        API_RECEIVE,    // Building an API command
        API_PROCESS,    // Processing an API command
        API_RESPOND,    // Sending an API response
        EVENT           // Sending an event
    };

    // Structure for a parsed API command
    struct SerialCommand {
        String method;      // GET/SET
        String path;        // API path
        std::map<String, String> params;  // Flatten key-value pairs
        bool valid = false;

        String toString() const {
            String result = method + " " + path;
            if (!params.empty()) {
                result += ":";
                bool first = true;
                for (const auto& [key, value] : params) {
                    if (!first) result += ",";
                    result += " " + key + "=" + value;
                    first = false;
                }
            }
            return result;
        }
    };

    // Structure for a pending API command
    struct PendingCommand {
        String command;     // Received command
        String response;    // Response to send
        size_t sendIndex;   // Position in the response
        bool processed;     // Indicates if the command has been processed
        
        PendingCommand() 
            : sendIndex(0)
            , processed(false) {}
            
        PendingCommand(const String& cmd) 
            : command(cmd)
            , sendIndex(0)
            , processed(false) {}
    };


    inline SerialCommand parseCommand(const String& line) const {
        SerialCommand cmd;
        SerialAPIFormatter::parseCommandLine(line, cmd.method, cmd.path, cmd.params);
        
        // Validate the command
        if (!cmd.method.isEmpty() && !cmd.path.isEmpty() && 
            (cmd.method == "GET" || cmd.method == "SET" || cmd.method == "LIST")) {
            cmd.valid = true;
        }
        
        return cmd;
    }

            

    void processStateMachine() {
        unsigned long now = millis();
        size_t processedChars = 0;
        
        // Check if we must return to mode NONE (timeout elapsed)
        if (now - _lastTxRx > MODE_RESET_DELAY) {
            switch (_mode) {
                case SerialMode::API_RECEIVE:
                    if (_apiBufferOverflow) {
                        _currentCommand.response = "< ERROR: error=command too long\n";
                        _mode = SerialMode::API_RESPOND;
                    } else if (_apiBufferIndex > 0) {
                        _currentCommand.response = "< ERROR: error=command timeout\n";
                        _mode = SerialMode::API_RESPOND;
                    } else {
                        _mode = SerialMode::NONE;
                    }
                    _apiBufferIndex = 0;
                    _apiBufferOverflow = false;
                    break;

                case SerialMode::PROXY_RECEIVE:
                case SerialMode::PROXY_SEND:
                    _mode = SerialMode::NONE;
                    break;

                case SerialMode::API_RESPOND:
                case SerialMode::EVENT:
                    if (_currentCommand.sendIndex >= _currentCommand.response.length()) {
                        if (_mode == SerialMode::EVENT) {
                            _currentCommand = PendingCommand();
                        }
                        _mode = SerialMode::NONE;
                    }
                    break;
            }
        }

        // Process according to the current state
        switch (_mode) {
            case SerialMode::NONE:
                // Send events only if there is no active command
                if (!_eventQueue.empty() && 
                    _currentCommand.command.isEmpty() && 
                    _currentCommand.response.isEmpty() && 
                    _currentCommand.sendIndex == 0) {
                    _mode = SerialMode::EVENT;
                    break;
                }
                
                // Check first if there is serial input
                if (_serial.available()) {
                    char c = _serial.read();
                    _lastTxRx = now;
                    if (c == '>') {
                        _mode = SerialMode::API_RECEIVE;
                        _apiBuffer[0] = c;
                        _apiBufferIndex = 1;
                    } else {
                        _mode = SerialMode::PROXY_RECEIVE;
                        proxy.writeToInput(c);
                    }
                }
                // Otherwise check if the proxy has data to send
                else if (proxy.availableForWrite()) {
                    _mode = SerialMode::PROXY_SEND;
                    _lastTxRx = now;
                }
                break;

            case SerialMode::PROXY_RECEIVE:
                while (_serial.available() && processedChars < RX_CHUNK_SIZE) {
                    char c = _serial.read();
                    proxy.writeToInput(c);
                    processedChars++;
                    _lastTxRx = now;
                }
                break;

            case SerialMode::PROXY_SEND:
                if (proxy.availableForWrite()) {
                    size_t bytesSent = 0;
                    while (proxy.availableForWrite() && bytesSent < TX_CHUNK_SIZE) {
                        int data = proxy.readOutput();
                        if (data == -1) break;
                        _serial.write((uint8_t)data);
                        bytesSent++;
                    }
                    if (bytesSent > 0) {
                        _serial.flush();
                    }
                    _lastTxRx = now;
                }
                break;

            case SerialMode::API_RECEIVE:
                while (_serial.available() && processedChars < RX_CHUNK_SIZE) {
                    char c = _serial.read();
                    _lastTxRx = now;
                    processedChars++;

                    if (_apiBufferOverflow) continue;

                    if (_apiBufferIndex < API_BUFFER_SIZE - 1) {
                        _apiBuffer[_apiBufferIndex] = c;
                        
                        if (c == '\n') {
                            // Message API complet
                            _apiBuffer[_apiBufferIndex] = '\0';
                            _currentCommand.command = String(_apiBuffer);
                            _mode = SerialMode::API_PROCESS;
                            break;
                        } else {
                            _apiBufferIndex++;
                        }
                    } else {
                        _apiBufferOverflow = true;
                    }
                }
                break;

            case SerialMode::API_PROCESS:
                if (!_currentCommand.processed) {
                    handleCommand(_currentCommand);
                    _currentCommand.processed = true;
                    _lastTxRx = now;
                }
                _mode = SerialMode::API_RESPOND;
                break;

            case SerialMode::API_RESPOND:
            case SerialMode::EVENT:
                // Only enter while loop if there is data to send
                if (_currentCommand.sendIndex < _currentCommand.response.length()) {
                    int sentChunks = 0;
                    // If MAX_TX_CHUNKS is 0, send all chunks (blocking)
                    while (_currentCommand.sendIndex < _currentCommand.response.length() && (MAX_TX_CHUNKS == 0 || sentChunks < MAX_TX_CHUNKS)) {
                        size_t remaining = _currentCommand.response.length() - _currentCommand.sendIndex;
                        size_t chunkSize = min(TX_CHUNK_SIZE, remaining);
                        _serial.write((uint8_t*)&_currentCommand.response[_currentCommand.sendIndex], chunkSize);
                        _serial.flush();
                        _currentCommand.sendIndex += chunkSize;
                        sentChunks++;
                    }
                    _lastTxRx = now; // Reset timer after sending data
                }
                break;
        }
    }

    String formatError(const String& method, const String& path, const String& error) const {
        return SerialAPIFormatter::formatError(method, path, error);
    }

    void handleCommand(PendingCommand& pendingCmd) {
        SerialCommand cmd;
        SerialAPIFormatter::parseCommandLine(pendingCmd.command, cmd.method, cmd.path, cmd.params);
        
        // Validate the command
        cmd.valid = !cmd.method.isEmpty() && !cmd.path.isEmpty() && 
            (cmd.method == "GET" || cmd.method == "SET" || cmd.method == "LIST");

        if (!cmd.valid) {
            pendingCmd.response = "< " + formatError(cmd.method, cmd.path, "invalid command");
            return;
        }

        // Handle GET api (simplified API doc) command separately
        if (cmd.method == "GET" && cmd.path == "api") {
            JsonArray methods;
            int methodCount = _apiServer.getAPIDoc(methods);
            pendingCmd.response = "< GET api\n";
            pendingCmd.response += SerialAPIFormatter::formatAPIList(methods);
            return;
        }

        // Get available methods once
        auto methods = _apiServer.getMethods("serial");
        auto methodIt = methods.find(cmd.path);
        if (methodIt == methods.end()) {
            pendingCmd.response = "< " + formatError(cmd.method, cmd.path, "method not found");
            return;
        }

        const APIMethod& method = methodIt->second;

        // Check authentication if required (with basic auth on Serial we only check the password)
        if (method.auth.enabled) {
            auto authPass = cmd.params.find("auth.password");
            
            if (authPass == cmd.params.end() || 
                authPass->second != method.auth.password) {
                pendingCmd.response = "< " + formatError(cmd.method, cmd.path, "authentication failed");
                return;
            }
            
            // Remove auth params before passing to handler
            cmd.params.erase("auth.password");
        }

        // Convert parameters to JsonObject if present
        StaticJsonDocument<512> doc;
        JsonObject args = doc.to<JsonObject>();
        
        for (const auto& [key, value] : cmd.params) {
            // Handle nested keys with dots
            JsonObject current = args;
            String remainingKey = key;
            
            while (remainingKey.indexOf('.') != -1) {
                String prefix = remainingKey.substring(0, remainingKey.indexOf('.'));
                remainingKey = remainingKey.substring(remainingKey.indexOf('.') + 1);
                
                if (!current.containsKey(prefix)) {
                    current = current.createNestedObject(prefix);
                } else {
                    current = current[prefix];
                }
            }
            
            // Set the final value
            current[remainingKey] = value;
        }

        // Execute the method
        StaticJsonDocument<512> responseDoc;
        JsonObject response = responseDoc.to<JsonObject>();
        
        if (_apiServer.executeMethod("serial",cmd.path, cmd.params.empty() ? nullptr : &args, response)) {
            pendingCmd.response = "< " + SerialAPIFormatter::formatResponse(cmd.method, cmd.path, response);
        } else {
            pendingCmd.response = "< " + formatError(cmd.method, cmd.path, "wrong request or parameters");
        }
    }

    Stream& _serial;

    // Chunk sizes for asynchronous serial communication
    static constexpr size_t RX_CHUNK_SIZE = 256;            // =1 full hardware buffer (~30ms @ 9600bps)
    static constexpr size_t TX_CHUNK_SIZE = 128;            // =1/2 hardware buffer
    static constexpr size_t MAX_TX_CHUNKS = 0;              // Maximal number of chunks to process at each write cycle (0 = all chunks, blocking)

    // Event queue
    std::queue<String> _eventQueue;                         // Queue of events to send
    static constexpr size_t QUEUE_SIZE = 10;                // Maximal number of events in the queue
    
    // API Serial buffer
    static constexpr size_t API_BUFFER_SIZE = 4096;         // Buffer size for API commands    
    char _apiBuffer[API_BUFFER_SIZE];                       // Buffer for API commands
    size_t _apiBufferIndex;                                 // Index in the buffer
    unsigned long _lastTxRx;                                // Last time a byte was sent or received
    
    // State machine
    SerialMode _mode = SerialMode::NONE;                    // Current state
    bool _apiBufferOverflow = false;                        // Indicates if the buffer has overflowed
    static constexpr unsigned long MODE_RESET_DELAY = 50;   // Grace time between modes
    PendingCommand _currentCommand;                         // Current command
    };

#endif // SERIALAPIENDPOINT_H