#ifndef APIENDPOINT_H
#define APIENDPOINT_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

// Forward declaration
class APIServer;

/**
 * @brief Base class for API endpoints
 * @brief An API endpoint is a class that handles a specific API protocol
 * @brief This class is abstract and must be implemented by concrete classes
 * @brief in order to handle a specific protocol (HTTP, WS, MQTT, Serial, Radio...)
 */
class APIEndpoint {
public:
    /**
     * @brief Capabilities bitmask for an API endpoint
     */
    enum Capability {
        GET = 1 << 0,   // GET method
        SET = 1 << 1,   // SET method
        EVT = 1 << 2    // EVT method (event)
    };

    /**
     * @brief Protocol description
     */
    struct Protocol {
        String name;
        uint8_t capabilities;
    };

    APIEndpoint(APIServer& apiServer) : _apiServer(apiServer) {}
    virtual ~APIEndpoint() = default;

    virtual void begin() = 0;
    virtual void poll() = 0;
    virtual void pushEvent(const String& event, const JsonObject& data) = 0;

    const std::vector<Protocol>& getProtocols() const { return _protocols; }

protected:
    /**
     * @brief Add a protocol to the endpoint
     * @param name Protocol name
     * @param capabilities Bitmask of capabilities
     */
    void addProtocol(const String& name, uint8_t capabilities) {
        _protocols.push_back({name, capabilities});
    }

    std::vector<Protocol> _protocols;
    APIServer& _apiServer;
};

#endif // APIENDPOINT_H 