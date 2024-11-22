#ifndef SERIALPROXY_H
#define SERIALPROXY_H

#include <Arduino.h>

class SerialProxy : public Stream {
public:
    static constexpr size_t OUTPUT_BUFFER_SIZE = 1024;
    static constexpr size_t INPUT_BUFFER_SIZE = 1024;

    SerialProxy() 
        : _outputReadIndex(0)
        , _outputWriteIndex(0)
        , _inputReadIndex(0)
        , _inputWriteIndex(0)
        , _initialized(false) {}

    void begin(unsigned long baud) {
        Serial.begin(baud);
        _initialized = true;
    }

    // Méthodes Stream pour la lecture du buffer d'entrée (pour l'application)
    int available() override {
        if (!_initialized) return 0;
        return (_inputWriteIndex - _inputReadIndex + INPUT_BUFFER_SIZE) % INPUT_BUFFER_SIZE;
    }

    int read() override {
        if (!_initialized || !available()) return -1;
        uint8_t data = _inputBuffer[_inputReadIndex];
        _inputReadIndex = (_inputReadIndex + 1) % INPUT_BUFFER_SIZE;
        return data;
    }

    int peek() override {
        if (!_initialized || !available()) return -1;
        return _inputBuffer[_inputReadIndex];
    }

    // Méthode d'écriture dans le buffer d'entrée (pour SerialAPIEndpoint)
    size_t writeToInput(uint8_t data) {
        if (!_initialized) return 0;
        size_t next = (_inputWriteIndex + 1) % INPUT_BUFFER_SIZE;
        if (next == _inputReadIndex) return 0; // Buffer plein
        _inputBuffer[_inputWriteIndex] = data;
        _inputWriteIndex = next;
        return 1;
    }

    // Méthode d'écriture dans le buffer de sortie (pour l'application)
    size_t write(uint8_t data) override {
        if (!_initialized) return 0;
        size_t next = (_outputWriteIndex + 1) % OUTPUT_BUFFER_SIZE;
        if (next == _outputReadIndex) return 0; // Buffer plein
        _outputBuffer[_outputWriteIndex] = data;
        _outputWriteIndex = next;
        return 1;
    }

    // Méthodes pour lire depuis le buffer de sortie (pour SerialAPIEndpoint)
    int availableForWrite() {
        if (!_initialized) return 0;
        return (_outputWriteIndex - _outputReadIndex + OUTPUT_BUFFER_SIZE) % OUTPUT_BUFFER_SIZE;
    }

    int readOutput() {
        if (!_initialized || !availableForWrite()) return -1;
        uint8_t data = _outputBuffer[_outputReadIndex];
        _outputReadIndex = (_outputReadIndex + 1) % OUTPUT_BUFFER_SIZE;
        return data;
    }

private:
    uint8_t _outputBuffer[OUTPUT_BUFFER_SIZE];
    uint8_t _inputBuffer[INPUT_BUFFER_SIZE];
    size_t _outputReadIndex;
    size_t _outputWriteIndex;
    size_t _inputReadIndex;
    size_t _inputWriteIndex;
    bool _initialized;
};

#endif // SERIALPROXY_H
