/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "SerialTwoWireSlave.h"
#include "SerialTwoWireDebug.h"

class SerialTwoWireMaster : public SerialTwoWireSlave
{
public:
    using SerialTwoWireSlave::SerialTwoWireSlave;
    using SerialTwoWireSlave::begin;
    using Stream::setTimeout;

    enum class EndTransmissionCode : uint8_t {
        SUCCESS = 0,
        DATA_TOO_LONG,
        NACK_ON_ADDRESS,
        NACK_ON_DATA,
        OTHER,
        TIMEOUT,
        INVALID_ADDRESS,
        OWN_ADDRESS,
        END_WITHOUT_BEGIN,

    };

public:
    void begin();

    // use setTimeout() to change the default timeout. default is set by the Stream class
    // and usually is 1000 milliseconds. the timeout is for receiving the complete the transmissions
    // yield() is called while waiting and the _onReadSerial() callback, if set
    uint8_t requestFrom(uint8_t address, uint8_t count, uint8_t stop = true);
    inline uint8_t requestFrom(int address, int count, int stop = true);

    void beginTransmission(uint8_t address);
    void beginTransmission(int address);
    uint8_t endTransmission(uint8_t stop = true);

    size_t available() const;
    size_t isAvailable();
    int readByte();
    int peekByte();
    size_t read(uint8_t *data, size_t length);
    size_t read(char *data, size_t length);

    template<class T>
    T &get(T &t) {
        if (read(reinterpret_cast<uint8_t *>(&t), sizeof(t)) != sizeof(t)) {
            t = T();
        }
        return t;
    }

    // avoid calling available/read/peek/readBytes since virtual function cannot be inline
    virtual int available() override;           // use isAvailabe()
    virtual int read() override;                // use readByte()
    virtual int peek() override;                // use peekByte()
#if defined(ESP8266) || defined(ESP32)
    virtual size_t readBytes(uint8_t *buffer, size_t length) override {     // use read()
        return read(buffer, length);
    }
    virtual size_t readBytes(char *buffer, size_t length) override {        // use read()
        return read(buffer, length);
    }
#else
    inline size_t readBytes(uint8_t *buffer, size_t length) {
        return read(buffer, length);
    }
    inline size_t readBytes(char *buffer, size_t length) {
        return read(buffer, length);
    }
#endif

    // this method must not be called from inside an ISR or reading serial data might be blocked
    // leading to read timeouts and blocking the ISR for the maximum timeout
    virtual void feed(uint8_t data);

protected:
    uint8_t _endTransmission(uint8_t stop);
    void _newLine();
    void _addBuffer(int data);
    void _processData();
    uint8_t _waitForResponse(uint8_t address, uint8_t count);

protected:
    const SerialTwoWireStream &readFrom() const;
    SerialTwoWireStream &readFrom();
    SerialTwoWireStream &_request();
};

#include "SerialTwoWireMaster.hpp"

#if HAVE_KFC_FIRMWARE_VERSION
#include <debug_helper_disable.h>
#endif
