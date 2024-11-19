#ifndef SERIALAPIENDPOINT_H
#define SERIALAPIENDPOINT_H

#include "APIServer.h"
#include "APIEndpoint.h"
#include <ArduinoJson.h>
#include <queue>

class SerialAPIEndpoint : public APIEndpoint {
public:
    SerialAPIEndpoint(APIServer& apiServer, Stream& serial = Serial) 
        : APIEndpoint(apiServer)
        , _serial(serial)
        , _lastUpdate(0)
    {
        // Declare supported protocols
        addProtocol("serial", GET | SET | EVT);
    }

    void begin() override {
        // Nothing to do, Serial will be initialized in the main application (setup())
    }

    void poll() override {
        // Process outgoing events queue
        unsigned long now = millis();
        if (now - _lastUpdate > SERIAL_POLL_INTERVAL) {
            processEventQueue();
            _lastUpdate = now;
        }

        // Process incoming commands
        while (_serial.available()) {
            String line = _serial.readStringUntil('\n');
            if (line.length() > 0) {
                handleCommand(line);
            }
        }
    }

    void pushEvent(const String& event, const JsonObject& data) override {
        if (_eventQueue.size() >= QUEUE_SIZE) {
            _eventQueue.pop();
        }
        _eventQueue.push(_formatter.formatEvent(event, data));
    }

private:
    // SerialCommand structure for parsed commands
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

    class SerialFormatter {
    public:
        String formatResponse(const String& path, const JsonObject& response) {
            String result = path + ":";
            bool first = true;
            
            std::function<void(const JsonObject&, const String&)> addParams = 
                [&](const JsonObject& obj, const String& prefix) {
                    for (JsonPair p : obj) {
                        String key = prefix.isEmpty() ? p.key().c_str() 
                                                    : prefix + "." + p.key().c_str();
                        if (p.value().is<JsonObject>()) {
                            addParams(p.value().as<JsonObject>(), key);
                        } else {
                            if (!first) result += ",";
                            result += " " + key + "=";
                            
                            // Handle different types
                            if (p.value().is<bool>())
                                result += p.value().as<bool>() ? "true" : "false";
                            else if (p.value().is<int>())
                                result += String(p.value().as<int>());
                            else if (p.value().is<float>())
                                result += String(p.value().as<float>());
                            else
                                result += p.value().as<String>();
                                
                            first = false;
                        }
                    }
                };
            
            addParams(response, "");
            return result;
        }

        String formatEvent(const String& event, const JsonObject& data) {
            return "EVT " + event + ": " + formatResponse("data", data);
        }

        SerialCommand parseCommand(const String& line) {
            SerialCommand cmd;
            
            // Basic validation
            if (line.length() < 4) return cmd;  // Minimum "> GET"
            
            // Remove prompt if present
            String input = line;
            if (line.startsWith("> ")) {
                input = line.substring(2);
            }
            
            // Parse method
            int spacePos = input.indexOf(' ');
            if (spacePos == -1) return cmd;
            
            cmd.method = input.substring(0, spacePos);
            if (cmd.method != "GET" && cmd.method != "SET" && cmd.method != "LIST") return cmd;
            
            // Parse path
            input = input.substring(spacePos + 1);
            int colonPos = input.indexOf(':');
            
            if (colonPos == -1) {
                // No parameters
                cmd.path = input;
            } else {
                // Has parameters
                cmd.path = input.substring(0, colonPos);
                String params = input.substring(colonPos + 1);
                
                // Split parameters
                int start = 0;
                bool inQuotes = false;
                
                for (size_t i = 0; i < params.length(); i++) {
                    char c = params[i];
                    if (c == '"') inQuotes = !inQuotes;
                    else if ((c == ',' || i == params.length() - 1) && !inQuotes) {
                        // Extract parameter
                        String param = params.substring(start, i == params.length() - 1 ? i + 1 : i);
                        param.trim();
                        
                        // Parse key=value
                        int equalPos = param.indexOf('=');
                        if (equalPos != -1) {
                            String key = param.substring(0, equalPos);
                            String value = param.substring(equalPos + 1);
                            key.trim();
                            value.trim();
                            
                            // Remove quotes if present
                            if (value.startsWith("\"") && value.endsWith("\"")) {
                                value = value.substring(1, value.length() - 1);
                            }
                            
                            cmd.params[key] = value;
                        }
                        
                        start = i + 1;
                    }
                }
            }
            
            cmd.valid = true;
            return cmd;
        }

        String formatAPIList(const JsonArray& methods) {
            String response = "api.methods:";
            
            for (JsonVariant method : methods) {
                response += "\n  " + method["path"].as<String>() + ":";
                response += "\n    type=" + method["type"].as<String>() + ",";
                response += "\n    desc=" + method["desc"].as<String>() + ",";
                
                // Add protocols
                response += "\n    protocols=";
                JsonArray protocols = method["protocols"].as<JsonArray>();
                bool first = true;
                for (JsonVariant proto : protocols) {
                    if (!first) response += "|";
                    response += proto.as<String>();
                    first = false;
                }
                
                // Add parameters if present
                if (method.containsKey("params")) {
                    response += ",\n";
                    for (JsonPair p : method["params"].as<JsonObject>()) {
                        response += "    params." + String(p.key().c_str()) + 
                                  "=" + p.value().as<String>() + ",\n";
                    }
                }
                
                // Add response parameters if present
                if (method.containsKey("response")) {
                    for (JsonPair p : method["response"].as<JsonObject>()) {
                        response += "    response." + String(p.key().c_str()) + 
                                  "=" + p.value().as<String>() + ",\n";
                    }
                }
                
                response += "\n";
            }
            
            return response;
        }
    };

    Stream& _serial;
    unsigned long _lastUpdate;
    std::queue<String> _eventQueue;
    SerialFormatter _formatter;
    
    static constexpr unsigned long SERIAL_POLL_INTERVAL = 50;
    static constexpr size_t QUEUE_SIZE = 10;

    void handleCommand(const String& line) {
        auto cmd = _formatter.parseCommand(line);
        if (!cmd.valid) {
            _serial.println("< error: invalid_command");
            return;
        }

        // Handle LIST command separately
        if (cmd.method == "LIST" && cmd.path == "api") {
            auto methods = _apiServer.getAPIDoc();
            _serial.print("< ");
            _serial.println(_formatter.formatAPIList(methods));
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
            _serial.print("< ");
            _serial.println(_formatter.formatResponse(cmd.path, response));
        } else {
            _serial.println("< error: invalid_request");
        }
    }

    void processEventQueue() {
        while (!_eventQueue.empty()) {
            _serial.print("< ");
            _serial.println(_eventQueue.front());
            _eventQueue.pop();
        }
    }
};

#endif // SERIALAPIENDPOINT_H