/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "SerialTwoWireDef.h"
#include "SerialTwoWireDebug.h"
#include "SerialTwoWireStream.h"

void serialEvent();

using namespace SerialTwoWireDef;

// to get a Stream object, use reinterpret_cast<Stream &>(Wire) and make sure that
// only virtual methods are called

class SerialTwoWireSlave : protected Stream {
public:
    using Stream::setTimeout;

    static constexpr uint8_t kReadValueOutOfRange = 0xff;

    static constexpr int kNoDataAvailable = -1;

    static constexpr size_t kTransmissionBufferMaxSize = kMaxTransmissionLength + 1;

public:
#if I2C_OVER_UART_USE_STD_FUNCTION
    typedef std::function<void(int)> onReceiveCallback;
    typedef std::function<void()> onRequestCallback;
    typedef std::function<void()> onReadSerialCallback;
#else
    typedef void (*onReceiveCallback)(int length);
    typedef void (*onRequestCallback)();
    typedef void (*onReadSerialCallback)();
#endif

    typedef enum : uint8_t {
        NONE,
        DISCARD,
        TRANSMIT,
        REQUEST,
    } CommandEnum_t;

    enum class OutStateType : uint8_t {
        NONE,
        LOCKED,
        FILL,
        FILLING,
        FILLED,
    };

    SerialTwoWireSlave();
#ifndef SERIALTWOWIRE_NO_GLOBALS
    SerialTwoWireSlave(Stream &serial, onReadSerialCallback callback = serialEvent);
#else
    SerialTwoWireSlave(Stream &serial, onReadSerialCallback callback = nullptr);
#endif
    SerialTwoWireSlave(SerialTwoWireSlave &&) = default;

    void setSerial(Stream &serial);

    void begin(uint8_t address);
    void end();

    inline void begin(int address) {
        begin((uint8_t)address);
    }

    inline void setClock(uint32_t) {}

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
#if defined(ESP8266)
    virtual size_t readBytes(uint8_t *buffer, size_t length) override;
    virtual size_t readBytes(char *buffer, size_t length) override;
#endif

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
    uint8_t _endTransmission(uint8_t stop);

    void _newLine();
    void _addBuffer(int data);
    int _parseData(bool lastByte = false);
    void _processData();
    void _discard();

    void _printHex(uint8_t data);
    void _printNibble(uint8_t nibble);
    void _println();

#if I2C_OVER_UART_ADD_CRC16
    void _printHexCrc(uint16_t crc);
    void _printHexUpdateCrc(uint8_t data);
#endif

protected:
    typedef struct Data_t  {
        union {
            struct {
                uint16_t _address : 8;                      // own address
                uint16_t _length : 8;                       // _buffer.length
            };
            uint16_t __init_val1;
        };
#if I2C_OVER_UART_ADD_CRC16
        uint16_t _crc;
#endif
        union {
            struct {
                uint16_t _command : 4;
                uint16_t _readFromOut : 1;                  // read from _out or _in
                uint16_t _crcMarker : 1;                    // crc marker received
                uint16_t _outState : 3;                     // _out buffer state
            };
            uint16_t __init_val2;
        };

        OutStateType _getOutState() const {
            return static_cast<OutStateType>(_outState);
        }

        void _setOutState(OutStateType state) {
            _outState = static_cast<uint16_t>(state);
        }

        bool _outCanWrite() const {
            return _getOutState() == OutStateType::LOCKED || _getOutState() == OutStateType::FILLING;
        }

        bool _outIsFilling() const {
            return _getOutState() == OutStateType::FILL || _getOutState() == OutStateType::FILLING;
        }

        Data_t() :
            __init_val1(0),
#if I2C_OVER_UART_ADD_CRC16
            _crc(~0),
#endif
            __init_val2(0)
        {
        }

    } Data_t;

protected:
    inline Data_t &data() {
        return _data;
    }

    inline Data_t &flags() {
        return _data;
    }

    inline SerialTwoWireStream &readFrom() {
        return _data._readFromOut ? _out : _in;
    }

    inline SerialTwoWireStream &_request() {
        return _out;
    }

protected:
    Data_t _data;
    uint8_t _buffer[kTransmissionBufferMaxSize];

    SerialTwoWireStream _in;            // incoming messages
    SerialTwoWireStream _out;           // output buffer for write

    onReceiveCallback _onReceive;
    onRequestCallback _onRequest;
    onReadSerialCallback _onReadSerial;

    Stream *_serial;
};

inline size_t SerialTwoWireSlave::write(const char *data, size_t length)
{
    return write(reinterpret_cast<const uint8_t *>(data), length);
}

inline void SerialTwoWireSlave::setSerial(Stream &serial)
{
    _serial = &serial;
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

inline void SerialTwoWireSlave::_discard()
{
    flags()._command = DISCARD;
    if (flags()._getOutState() == OutStateType::FILLING) {
        flags()._setOutState(OutStateType::NONE);
    }
}

inline void SerialTwoWireSlave::_printHex(uint8_t data)
{
    _printNibble(data >> 4);
    _printNibble(data & 0xf);
}

inline void SerialTwoWireSlave::_println()
{
    _serial->write('\n');
}

inline void SerialTwoWireSlave::_printNibble(uint8_t nibble)
{
    _serial->write(nibble < 0xa ? (nibble + '0') : (nibble + ('a' - 0xa)));
}

#if I2C_OVER_UART_ADD_CRC16

inline void SerialTwoWireSlave::_printHexCrc(uint16_t crc)
{
    _serial->write(kCrcStartChar);
    // not big endian but for strtoul() ...
    _printHex(crc >> 8);
    _printHex((uint8_t)crc);
    _println();
}

inline void SerialTwoWireSlave::_printHexUpdateCrc(uint8_t data)
{
    flags()._crc = _crc16_update(flags()._crc, data);
    _printHex(data);
}

#endif
