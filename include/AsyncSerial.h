#ifndef ASYNCSERIAL_H
#define ASYNCSERIAL_H

#include <Arduino.h>
#include <vector>

using Bytes = std::vector<uint8_t>;
static constexpr size_t MAX_PROXIES = 8;

// ============================== RingBuffer ==============================
template<typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t size) 
        : _size(size)
        , _buffer(new T[size])
        , _readIndex(0)
        , _writeIndex(0)
        , _count(0) {}

    ~RingBuffer() {
        delete[] _buffer;
    }

    bool write(T data) {
        if (_count >= _size) return false;
        _buffer[_writeIndex] = data;
        _writeIndex = (_writeIndex + 1) % _size;
        _count++;
        return true;
    }

    bool write(const T* data, size_t length) {
        if (_count + length > _size) return false;
        for (size_t i = 0; i < length; i++) {
            _buffer[_writeIndex] = data[i];
            _writeIndex = (_writeIndex + 1) % _size;
        }
        _count += length;
        return true;
    }

    bool read(T& data) {
        if (_count == 0) return false;
        data = _buffer[_readIndex];
        _readIndex = (_readIndex + 1) % _size;
        _count--;
        return true;
    }

    bool peek(T& data) const {
        if (_count == 0) return false;
        data = _buffer[_readIndex];
        return true;
    }

    size_t available() const { return _count; }
    void clear() { _readIndex = _writeIndex = _count = 0; }

private:
    const size_t _size;
    T* const _buffer;
    size_t _readIndex;
    size_t _writeIndex;
    size_t _count;
};

// ============================== SerialProxy ==============================
struct SerialProxyConfig {
    size_t rxBufferSize = 1024;    
    size_t txBufferSize = 1024;    
    AsyncSerial::TransmitMode mode = AsyncSerial::TransmitMode::BEST_EFFORT;
    uint32_t chunkSize = 32;
    uint32_t timeout = 100;
    uint32_t interMessageDelay = 5;
};

class SerialProxy : public Stream {
public:
    explicit SerialProxy(const SerialProxyConfig& config = SerialProxyConfig{})
        : _rxBuffer(config.rxBufferSize)
        , _txBuffer(config.txBufferSize)
        , _config(config) {}

    const SerialProxyConfig& getConfig() const { return _config; }

    // Stream interface
    int available() override { return _rxBuffer.available(); }
    
    int read() override {
        uint8_t data;
        return _rxBuffer.read(data) ? data : -1;
    }
    
    int peek() override {
        uint8_t data;
        return _rxBuffer.peek(data) ? data : -1;
    }

    size_t write(uint8_t data) override {
        return _txBuffer.write(data) ? 1 : 0;
    }

    // Block write methods
    bool write(const Bytes& data) {
        return _txBuffer.write(data.data(), data.size());
    }
    
    bool write(const String& data) {
        return _txBuffer.write((const uint8_t*)data.c_str(), data.length());
    }

    void flush() override {}

    // Interface for AsyncSerial
    bool pushToRx(uint8_t data) { return _rxBuffer.write(data); }
    bool readFromTx(uint8_t& data) { return _txBuffer.read(data); }
    size_t txAvailable() const { return _txBuffer.available(); }

private:
    RingBuffer<uint8_t> _rxBuffer;  // AsyncSerial -> Application
    RingBuffer<uint8_t> _txBuffer;  // Application -> AsyncSerial
    SerialProxyConfig _config;
};

// ============================== AsyncSerial ==============================
class AsyncSerial {
public:
    enum class TransmitMode {
        BEST_EFFORT,
        SYNCHRONOUS
    };

    struct SerialProxyConfig {
        size_t rxBufferSize = 1024;    
        size_t txBufferSize = 1024;    
        TransmitMode mode = TransmitMode::BEST_EFFORT;
        uint32_t chunkSize = 0;        // 0 = send entire buffer
        uint32_t txResponseTimeout = 100; // Temps d'attente pour recevoir une réponse
        uint32_t rxRequestTimeout = 100;  // Temps réservé pour répondre à une requête
        uint32_t interMessageDelay = 5;
    };

private:
    enum class State {
        IDLE,
        READ,
        WRITE,
        FLUSH    // Nouvel état pour le flush bloquant
    };

    struct ProxyState {
        SerialProxy* proxy;
        SerialProxyConfig config;
        unsigned long lastTxTime;
        unsigned long lastRxTime;
        bool isWaitingResponse;   // En attente d'une réponse
        bool isProcessingRequest; // En train de traiter une requête
    };

    State _state = State::IDLE;
    ProxyState _proxies[MAX_PROXIES];
    size_t _proxyCount = 0;
    SerialProxy* _flushingProxy = nullptr;  // Proxy qui a demandé le flush

public:
    void poll() {
        unsigned long now = millis();

        switch (_state) {
            case State::IDLE:
                if (Serial.available()) {
                    _state = State::READ;
                } else {
                    for (size_t i = 0; i < _proxyCount; i++) {
                        if (_proxies[i].proxy->txAvailable() > 0) {
                            _state = State::WRITE;
                            break;
                        }
                    }
                }
                break;

            case State::READ:
                while (Serial.available()) {
                    uint8_t data = Serial.read();
                    for (size_t i = 0; i < _proxyCount; i++) {
                        auto& state = _proxies[i];
                        state.proxy->pushToRx(data);
                        
                        if (state.config.mode == TransmitMode::SYNCHRONOUS) {
                            state.lastRxTime = now;
                            state.isProcessingRequest = true;  // Démarre le timer de réponse
                        }
                    }
                }
                _state = State::IDLE;
                break;

            case State::WRITE:
                for (size_t i = 0; i < _proxyCount; i++) {
                    auto& state = _proxies[i];
                    
                    // 1. Vérifier si un proxy est en train de traiter une requête
                    if (state.config.mode == TransmitMode::SYNCHRONOUS && state.isProcessingRequest) {
                        if (now - state.lastRxTime <= state.config.rxRequestTimeout) {
                            // Donner la priorité au proxy qui doit répondre
                            _state = State::IDLE;
                            return;
                        }
                        state.isProcessingRequest = false;
                    }

                    // 2. Vérifier si un proxy attend une réponse
                    if (state.config.mode == TransmitMode::SYNCHRONOUS && state.isWaitingResponse) {
                        if (now - state.lastTxTime <= state.config.txResponseTimeout) {
                            // Attendre la réponse avant de traiter d'autres messages
                            _state = State::IDLE;
                            return;
                        }
                        state.isWaitingResponse = false;
                    }

                    // Traitement normal
                    if (now - state.lastTxTime < state.config.interMessageDelay) {
                        continue;
                    }

                    if (state.proxy->txAvailable() > 0) {
                        size_t toSend = state.config.chunkSize == 0 ? 
                                       state.proxy->txAvailable() : 1;
                        
                        for (size_t j = 0; j < toSend; j++) {
                            uint8_t data;
                            if (state.proxy->readFromTx(data)) {
                                Serial.write(data);
                            }
                        }
                        Serial.flush();
                        state.lastTxTime = now;

                        if (state.config.mode == TransmitMode::SYNCHRONOUS) {
                            state.isWaitingResponse = true;
                            _state = State::IDLE;
                            return;
                        }
                    }
                }
                _state = _flushingProxy ? State::FLUSH : State::IDLE;
                break;

            case State::FLUSH:
                // Envoie tout le buffer du proxy qui a demandé le flush
                while (_flushingProxy->txAvailable() > 0) {
                    uint8_t data;
                    if (_flushingProxy->readFromTx(data)) {
                        Serial.write(data);
                    }
                }
                Serial.flush();
                _flushingProxy = nullptr;
                _state = State::IDLE;
                break;
        }
    }

    void flush(SerialProxy* proxy) {
        // Attend la fin de la transmission en cours
        while (_state == State::WRITE) {
            poll();
        }
        
        // Force l'envoi immédiat du buffer du proxy
        _flushingProxy = proxy;
        _state = State::FLUSH;
        
        // Attend que le flush soit terminé
        while (_state == State::FLUSH) {
            poll();
        }
    }
};

#endif // ASYNCSERIAL_H