#ifndef API_SERVER_H
#define API_SERVER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

// Forward declaration
class RPCServer;

class APIServer {
public:
    enum Capability {
        GET = 1 << 0,
        SET = 1 << 1,
        EVT = 1 << 2
    };

    struct Protocol {
        String name;
        uint8_t capabilities;
    };

    APIServer(RPCServer& rpcServer) : _rpcServer(rpcServer) {}
    virtual ~APIServer() = default;

    virtual void begin() = 0;
    virtual void poll() = 0;
    
    virtual void handleGet(const String& path, const JsonObject* args, JsonObject& response) = 0;
    virtual void handleSet(const String& path, const JsonObject& args, JsonObject& response) = 0;
    virtual void pushEvent(const String& event, const JsonObject& data) = 0;

    const std::vector<Protocol>& getProtocols() const { return _protocols; }

protected:
    void addProtocol(const String& name, uint8_t capabilities) {
        _protocols.push_back({name, capabilities});
    }

    std::vector<Protocol> _protocols;
    RPCServer& _rpcServer;
};

#endif // API_SERVER_H 