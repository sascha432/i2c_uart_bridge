/**
 * Author: sascha_lammers@gmx.de
 */

// I2C over UART emulation

#pragma once

#include <Arduino.h>
#if SERIALTWOWIRE_USE_BUFFERSTREAM
#include <BufferStream.h>
using SerialTwoWireStream = BufferStream;
#else
#include <StreamString.h>
using SerialTwoWireStream = StreamString;
#endif

#ifndef HAVE_I2C_SCANNER
#define HAVE_I2C_SCANNER                        0
#endif

#ifndef DEBUG_SERIALTWOWIRE
#define DEBUG_SERIALTWOWIRE                     0
#endif

#ifndef I2C_OVER_UART_PREFIX_TRANSMIT
#define I2C_OVER_UART_PREFIX_TRANSMIT           "+I2CT="
#endif

#ifndef I2C_OVER_UART_PREFIX_REQUEST
#define I2C_OVER_UART_PREFIX_REQUEST            "+I2CR="
#endif

#ifndef I2C_OVER_UART_PREFIX_MAX_LEN
#define I2C_OVER_UART_PREFIX_MAX_LEN            7       // max. strlen() + 1 of I2C_OVER_UART_PREFIX_TRANSMIT and I2C_OVER_UART_PREFIX_REQUEST
#endif

class Serial;

class SerialTwoWire : public Stream {
public:
    static constexpr size_t kMaxBufferSize = (I2C_OVER_UART_PREFIX_MAX_LEN + 1) & ~1;

public:
    typedef std::function<void(int)> onReceiveCallback;
    typedef std::function<void()> onRequestCallback;
    typedef std::function<void()> onReadSerialCallback;

    typedef enum : int8_t {
        NONE,
        STOP_LINE,
        TRANSMIT,
        REQUEST,
    } CommandEnum_t;

    SerialTwoWire();
    SerialTwoWire(Stream &serial);
    void setSerial(Stream &serial);

    inline void begin(uint8_t address) {
        end();
        _address = address;
    }
    inline void begin(int address) {
        begin((uint8_t)address);
    }

    inline void begin() {
        end();
    }
    void end();


    inline void setClock(uint32_t) {
    }

    void beginTransmission(uint8_t address);
    inline void beginTransmission(int address) {
        beginTransmission((uint8_t)address);
    }

    uint8_t endTransmission(uint8_t stop = true);

    uint8_t requestFrom(uint8_t address, uint8_t count, uint8_t stop = true);
    inline uint8_t requestFrom(int address, int count, int stop = true) {
        return requestFrom((uint8_t)address, (uint8_t)count, (uint8_t)stop);
    }

    virtual size_t write(uint8_t data);
    virtual size_t write(const uint8_t *data, size_t length);
    virtual int available(void);
    virtual int read(void);
    virtual int peek(void);
    virtual void flush(void) {}

    void onReceive(onReceiveCallback callback);
    void onRequest(onRequestCallback callback);
    void onReadSerial(onReadSerialCallback callback);

    inline size_t write(unsigned long n) {
        return write((uint8_t)n);
    }
    inline size_t write(long n) {
        return write((uint8_t)n);
    }
    inline size_t write(unsigned int n) {
        return write((uint8_t)n);
    }
    inline size_t write(int n) {
        return write((uint8_t)n);
    }

    using Print::write;

    // feed single bytes into the bridge
    void feed(uint8_t data);
    // process collected data and start new line
    void newLine();
    // stop parsing current line
    void stopLine();

public:
    void _serialEvent();

private:
    uint8_t _waitForResponse();
    void _addBuffer();
    void _processData();
    void _printHex(uint8_t data);

    uint32_t _address: 8;
    uint32_t _recvAddress: 8;
    uint32_t _length: 14;
    uint32_t _command: 2;
    uint8_t _buffer[kMaxBufferSize];
    SerialTwoWireStream _in;
    SerialTwoWireStream _out;
    Stream &_serial;

    onReceiveCallback _onReceive;
    onRequestCallback _onRequest;
    onReadSerialCallback _onReadSerial;
};

inline void SerialTwoWire::setSerial(Stream &serial)
{
    _serial = serial;
}

inline void SerialTwoWire::stopLine()
{
    _command = STOP_LINE;
}

inline void SerialTwoWire::onReceive(onReceiveCallback callback)
{
    _onReceive = callback;
}

inline void SerialTwoWire::onRequest(onRequestCallback callback)
{
    _onRequest = callback;
}

inline void SerialTwoWire::onReadSerial(onReadSerialCallback callback)
{
    _onReadSerial = callback;
}

inline void SerialTwoWire::_printHex(uint8_t data)
{
    _serial.print(data >> 4, HEX);
    _serial.print(data & 0xf, HEX);
}

#ifndef SERIALTWOWIRE_NO_GLOBALS
extern SerialTwoWire Wire;
#endif
