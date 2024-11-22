#ifndef SERIALAPIENDPOINT_H
#define SERIALAPIENDPOINT_H

#include <ArduinoJson.h>
#include <queue>
#include "APIServer.h"
#include "APIEndpoint.h"
#include "SerialProxy.h"
#include "SerialFormatter.h"

class SerialAPIEndpoint : public APIEndpoint {
public:
    static SerialProxy proxy;  // Public static proxy

    SerialAPIEndpoint(APIServer& apiServer, Stream& serial = Serial) 
        : APIEndpoint(apiServer)
        , _serial(serial)
        , _lastTxRx(0)
        , _bufferIndex(0)
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
        _eventQueue.push(_formatter.formatEvent(event, data));
    }

private:
    enum class SerialMode {
        NONE,           // En attente du premier caractère
        PROXY_RECEIVE,  // Réception de données pour le proxy
        PROXY_SEND,     // Envoi de données du proxy
        API_RECEIVE,    // Construction d'une commande API
        API_PROCESS,    // Traitement de la commande API
        API_RESPOND,    // Envoi de la réponse API
        EVENT           // Envoi d'un événement
    };

    // Structure pour une commande API parsée
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

    // Structure pour une commande API en attente
    struct PendingCommand {
        String command;     // Commande reçue
        String response;    // Réponse à envoyer
        size_t sendIndex;   // Position dans la réponse
        bool processed;     // Indique si la commande a été traitée
        
        PendingCommand() 
            : sendIndex(0)
            , processed(false) {}
            
        PendingCommand(const String& cmd) 
            : command(cmd)
            , sendIndex(0)
            , processed(false) {}
    };


    inline SerialCommand parseCommand(const String& line) {
        SerialCommand cmd;
        _formatter.parseCommandLine(line, cmd.method, cmd.path, cmd.params);
        
        // Valider la commande
        if (!cmd.method.isEmpty() && !cmd.path.isEmpty() && 
            (cmd.method == "GET" || cmd.method == "SET" || cmd.method == "LIST")) {
            cmd.valid = true;
        }
        
        return cmd;
    }

            

    void processStateMachine() {
        unsigned long now = millis();
        size_t processedChars = 0;
        
        // Vérifier si on doit revenir en mode NONE (temps de répit écoulé)
        if (now - _lastTxRx > MODE_RESET_DELAY) {
            switch (_mode) {
                case SerialMode::API_RECEIVE:
                    if (_bufferOverflow) {
                        _currentCommand.response = "< ERROR: error=command too long\n";
                        _mode = SerialMode::API_RESPOND;
                    } else if (_bufferIndex > 0) {
                        _currentCommand.response = "< ERROR: error=command timeout\n";
                        _mode = SerialMode::API_RESPOND;
                    } else {
                        _mode = SerialMode::NONE;
                    }
                    _bufferIndex = 0;
                    _bufferOverflow = false;
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

        // Traitement selon l'état
        switch (_mode) {
            case SerialMode::NONE:
                // Vérifier s'il y a des événements à envoyer UNIQUEMENT s'il n'y a pas de commande active
                if (!_eventQueue.empty() && 
                    _currentCommand.command.isEmpty() && 
                    _currentCommand.response.isEmpty() && 
                    _currentCommand.sendIndex == 0) {
                    _mode = SerialMode::EVENT;
                    break;
                }
                
                // Vérifier d'abord s'il y a des entrées série
                if (_serial.available()) {
                    char c = _serial.read();
                    _lastTxRx = now;
                    if (c == '>') {
                        _mode = SerialMode::API_RECEIVE;
                        _buffer[0] = c;
                        _bufferIndex = 1;
                    } else {
                        _mode = SerialMode::PROXY_RECEIVE;
                        _proxy.writeToInput(c);
                    }
                }
                // Sinon vérifier si le proxy a des données à envoyer
                else if (_proxy.availableForWrite()) {
                    _mode = SerialMode::PROXY_SEND;
                    _lastTxRx = now;
                }
                break;

            case SerialMode::PROXY_RECEIVE:
                while (_serial.available() && processedChars < RX_CHUNK_SIZE) {
                    char c = _serial.read();
                    _proxy.writeToInput(c);
                    processedChars++;
                    _lastTxRx = now;
                }
                break;

            case SerialMode::PROXY_SEND:
                if (_proxy.availableForWrite()) {
                    size_t bytesSent = 0;
                    while (_proxy.availableForWrite() && bytesSent < TX_CHUNK_SIZE) {
                        int data = _proxy.readOutput();
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

                    if (_bufferOverflow) continue;

                    if (_bufferIndex < SERIAL_BUFFER_SIZE - 1) {
                        _buffer[_bufferIndex] = c;
                        
                        if (c == '\n') {
                            // Message API complet
                            _buffer[_bufferIndex] = '\0';
                            _currentCommand.command = String(_buffer);
                            _mode = SerialMode::API_PROCESS;
                            break;
                        } else {
                            _bufferIndex++;
                        }
                    } else {
                        _bufferOverflow = true;
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

    String formatError(const String& method, const String& path, const String& error) {
        return _formatter.formatError(method, path, error);
    }

    void handleCommand(PendingCommand& pendingCmd) {
        SerialCommand cmd;
        _formatter.parseCommandLine(pendingCmd.command, cmd.method, cmd.path, cmd.params);
        
        // Valider la commande
        cmd.valid = !cmd.method.isEmpty() && !cmd.path.isEmpty() && 
            (cmd.method == "GET" || cmd.method == "SET" || cmd.method == "LIST");

        if (!cmd.valid) {
            pendingCmd.response = "< " + formatError(cmd.method, cmd.path, "invalid command");
            return;
        }

        // Handle GET api command separately
        if (cmd.method == "GET" && cmd.path == "api") {
            JsonArray methods;
            int methodCount = _apiServer.getAPIDoc(methods);
            pendingCmd.response = "< GET api\n";
            pendingCmd.response += _formatter.formatAPIList(methods);
            return;
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
        
        if (_apiServer.executeMethod(cmd.path, cmd.params.empty() ? nullptr : &args, response)) {
            pendingCmd.response = "< " + _formatter.formatResponse(cmd.method, cmd.path, response);
        } else {
            pendingCmd.response = "< " + formatError(cmd.method, cmd.path, "wrong request or parameters");
        }
    }

    Stream& _serial;
    SerialProxy _proxy;
    std::queue<String> _eventQueue;
    SerialFormatter _formatter;
    
    static constexpr size_t SERIAL_BUFFER_SIZE = 4096; 
    char _buffer[SERIAL_BUFFER_SIZE];
    size_t _bufferIndex;
    unsigned long _lastTxRx;
    
    static constexpr size_t QUEUE_SIZE = 10;
    static constexpr size_t RX_CHUNK_SIZE = 256; // =1 full hardware buffer (~30ms @ 9600bps)
    static constexpr size_t TX_CHUNK_SIZE = 128; // =1/2 hardware buffer
    static constexpr size_t MAX_TX_CHUNKS = 0; // Maximal number of chunks to process at each write cycle (0 = all chunks, blocking)

    SerialMode _mode = SerialMode::NONE;
    bool _bufferOverflow = false;
    static constexpr unsigned long MODE_RESET_DELAY = 50;  // Temps de répit entre les modes

    PendingCommand _currentCommand;

    // Limit number of characters processed per cycle to avoid blocking the thread on long messages
    
};

// Définition du proxy statique
SerialProxy SerialAPIEndpoint::proxy;

#endif // SERIALAPIENDPOINT_H