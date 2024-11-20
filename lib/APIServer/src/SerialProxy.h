#ifndef SERIALPROXY_H
#define SERIALPROXY_H

#include <Arduino.h>

class SerialProxy : public Stream {
public:
    static constexpr size_t BUFFER_SIZE = 1024;

    SerialProxy() : _readIndex(0), _writeIndex(0), _initialized(false) {}

    void begin(unsigned long baud) {
        Serial.begin(baud);
        _initialized = true;
    }

    // Méthodes Stream
    int available() override {
        if (!_initialized) return 0;
        return (_writeIndex - _readIndex + BUFFER_SIZE) % BUFFER_SIZE;
    }

    int read() override {
        if (!_initialized || !available()) return -1;
        uint8_t data = _buffer[_readIndex];
        _readIndex = (_readIndex + 1) % BUFFER_SIZE;
        return data;
    }

    int peek() override {
        if (!_initialized || !available()) return -1;
        return _buffer[_readIndex];
    }

    // Méthodes Print
    size_t write(uint8_t data) override {
        if (!_initialized) return 0;
        size_t next = (_writeIndex + 1) % BUFFER_SIZE;
        if (next == _readIndex) return 0; // Buffer plein
        _buffer[_writeIndex] = data;
        _writeIndex = next;
        return 1;
    }

private:
    uint8_t _buffer[BUFFER_SIZE];
    size_t _readIndex;
    size_t _writeIndex;
    bool _initialized;
};

#endif // SERIALPROXY_H
