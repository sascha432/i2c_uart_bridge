/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino.h>
#include "SerialTwoWireDef.h"
#include "SerialTwoWireDebug.h"
#include "SerialTwoWireStream.h"

class SerialTwoWireSlave : public Stream {
public:
    static constexpr size_t kMaxBufferSize = kSerialTwoWireMaxCommandLength + 1;
    static constexpr uint8_t kFillingRequest = 0xff;
    static constexpr uint8_t kFinishedRequest = 0x00;
    static constexpr uint8_t kReadValueOutOfRange = 0xff;
    static constexpr uint8_t kRequestCommandMaxLength = I2C_OVER_UART_REQUEST_COMMAND_LENGTH;
    static constexpr uint8_t kTransmissionMaxLength = I2C_OVER_UART_MAX_INPUT_LENGTH;
    static constexpr int kNoDataAvailable = -1;

public:
#if __AVR__
    typedef void (*onReceiveCallback)(int length);
    typedef void (*onRequestCallback)();
    typedef void (*onReadSerialCallback)();
#else
    typedef std::function<void(int)> onReceiveCallback;
    typedef std::function<void()> onRequestCallback;
    typedef std::function<void()> onReadSerialCallback;
#endif

    typedef enum : uint8_t {
        NONE,
        DISCARD,
        TRANSMIT,
        REQUEST,
    } CommandEnum_t;

    SerialTwoWireSlave();
    SerialTwoWireSlave(Stream &serial);
    SerialTwoWireSlave(Stream &serial, onReadSerialCallback callback);

    void setSerial(Stream &serial);

    inline void begin(uint8_t address) {
        end();
        _address = address;
    }
    inline void begin(int address) {
        begin((uint8_t)address);
    }

    void end();

    inline void setClock(uint32_t) {
    }

    void beginTransmission(uint8_t address);
    inline void beginTransmission(int address) {
        beginTransmission((uint8_t)address);
    }
    uint8_t endTransmission(uint8_t stop = true);

    virtual size_t write(uint8_t data) override;
    virtual size_t write(const uint8_t *data, size_t length) override;
    size_t write(const char *data, size_t length);

    virtual int available() override;
    virtual int read() override;
    virtual int peek() override;
    virtual size_t readBytes(uint8_t *buffer, size_t length);
    virtual size_t readBytes(char *buffer, size_t length);

    virtual void flush() override {}

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
    virtual void feed(uint8_t data);

public:
    void _serialEvent();

protected:
    size_t _write(const uint8_t *data, size_t length);
    void _newLine();
    void _addBuffer(int data);
    int _parseData(bool lastByte = false);
    void _processData();
    void _printHex(uint8_t data);
    void _discard();

protected:
    uint8_t _address;                   // own address
    uint8_t _length;                    // _buffer.length
#if I2C_OVER_UART_ADD_CRC16
    uint8_t _command: 7;
    uint8_t _crcMarker: 1;
    uint16_t _crc;
#else
    CommandEnum_t _command;
#endif
    uint8_t _buffer[kMaxBufferSize];

    SerialTwoWireStream _in;            // incoming messages
    SerialTwoWireStream _out;           // output buffer for write

    onReceiveCallback _onReceive;
    onRequestCallback _onRequest;
    onReadSerialCallback _onReadSerial;

#if DEBUG_SERIALTWOWIRE && DEBUG_BUFFER_ALLOC
    uint32_t _transmissions;
#endif

    Stream &_serial;
};

inline void SerialTwoWireSlave::setSerial(Stream &serial)
{
    _serial = serial;
}

inline void SerialTwoWireSlave::_discard()
{
    _command = DISCARD;
}

inline void SerialTwoWireSlave::onReceive(onReceiveCallback callback)
{
    _onReceive = callback;
}

inline void SerialTwoWireSlave::onRequest(onRequestCallback callback)
{
    _onRequest = callback;
}

inline void SerialTwoWireSlave::onReadSerial(onReadSerialCallback callback)
{
    _onReadSerial = callback;
}

inline size_t SerialTwoWireSlave::write(const char *data, size_t length)
{
    return _out.write(reinterpret_cast<const uint8_t *>(data), length);
}

inline size_t SerialTwoWireSlave::_write(const uint8_t *data, size_t length)
{
    return _out.write(data, length);
}

inline void SerialTwoWireSlave::_printHex(uint8_t data)
{
    _serial.print(data >> 4, HEX);
    _serial.print(data & 0xf, HEX);
}
