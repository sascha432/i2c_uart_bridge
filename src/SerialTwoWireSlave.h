/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "SerialTwoWire.h"
#include "SerialTwoWireDef.h"
#include "SerialTwoWireStream.h"
#include "SerialTwoWireDebug.h"

#ifndef SERIALTWOWIRE_NO_GLOBALS

extern "C" void serialEvent();

#endif


using namespace SerialTwoWireDef;

class SerialTwoWireSlave : public Stream {
public:
    using Stream::setTimeout;

    // return code for parseData()
    static constexpr int kNoDataAvailable = -1;

    // parse buffer size
    static constexpr size_t kCommandBufferSize = kCommandMaxLength + 1;

    static constexpr uint8_t kMinAddress = 0x00;
    // up to 0xf0 the rest is reserved
    static constexpr uint8_t kMaxAddress = 0x7f;
    // address assigned to master without slave mode
    static constexpr uint8_t kNotInitializedAddress = 0xfe;
    static constexpr uint8_t kMasterAddress = 0xff;

    static bool isValidAddress(uint8_t address);

public:
#if I2C_OVER_UART_USE_STD_FUNCTION
    using onReceiveCallback = typedef std::function<void(int)>;
    using onRequestCallback = typedef std::function<void()>;
    using onReadSerialCallback = typedef std::function<void()>;
#else
    typedef void (*onReceiveCallback)(int length);
    typedef void (*onRequestCallback)();
    typedef void (*onReadSerialCallback)();
#endif

protected:
    typedef enum : uint8_t {
        NONE,
        // discard transmission
        DISCARD,
        // transmit from slave after requestFrom() -> _request(_out) buffer
        // the data is available for read until a new request or transmission is started
        // the data will be removed from the buffer at this point
        // or
        // transmit from master -> _in buffer
        // data can only be read/written inside the onReceive callback
        TRANSMIT,
        // request from master -> _in buffer, followed by a response -> _out
        // data can only be read/written inside the onRequest callback
        REQUEST,
        // if I2C_OVER_UART_ENABLE_DISCARD_COMMAND is enabled, commands that do not
        // match are sent to onReceive
        SEND_DISCARDED,
    } CommandEnum_t;

    enum class OutStateType : uint8_t {
        NONE,
        LOCKED,
        FILL,
        FILLING,
        FILLED,
    };


    typedef struct Data_t {
        union {
            struct {
                uint16_t _address : 8;                      // own address
                uint16_t _length : 8;                       // _buffer.length
            };
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
                uint16_t _inState : 1;                      // _in buffer state
            };
        };

        uint8_t _getAddress() const {
            return _address;
        }

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
            _address(kNotInitializedAddress),
            _length(0),
#if I2C_OVER_UART_ADD_CRC16
            _crc(~0),
#endif
            _command(0),
            _readFromOut(true),
            _crcMarker(0),
            _outState(0),
            _inState(false)
        {
        }

    } Data_t;

public:
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

    void begin(int address);

    void setClock(uint32_t);

    virtual size_t write(uint8_t data) override;
    virtual size_t write(const uint8_t *data, size_t length) override;
    size_t write(const char *data, size_t length);

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
    virtual size_t readBytes(uint8_t *data, size_t length) override {       // use read
        return read(data, length);
    }
    virtual size_t readBytes(char *data, size_t length) override {          // use read
        return read(data, length);
    }
#else
    inline size_t readBytes(uint8_t *data, size_t length) {
        return read(data, length);
    }
    inline size_t readBytes(char *data, size_t length) {
        return read(data, length);
    }
#endif

    virtual void flush() override {}

    void onReceive(onReceiveCallback callback);
    void onRequest(onRequestCallback callback);
    void onReadSerial(onReadSerialCallback callback);

    size_t write(unsigned long n);
    size_t write(long n);
    size_t write(unsigned int n);
    size_t write(int n);

    void setAllocMinSize(uint8_t size);
    void releaseBuffers();

public:
    // as soon as data is available on the Serial port used for the bridge,
    // the data needs to be fed into the object. this should be executed in the
    // loop function and not inside any ISR. if using the master, the method
    // must not called from inside an ISR
    virtual void feed(uint8_t data);

    Stream *getSerial() const;
    Stream &getSerial();

protected:
    void _end();
    void _newLine();
    void _addBuffer(int data);
    int _parseData(bool lastByte = false);
    void _processData();
    void _discard();
    void _sendAndDiscard();
    void _preProcess();
    void _cleanup();
    void _sendNack(uint8_t address);

    size_t _printHex(uint8_t data);
    size_t _printNibble(uint8_t nibble);
    size_t _println();

#if DEBUG_SERIALTWOWIRE
    static inline uint32_t __inline_get_time_diff(uint32_t start, uint32_t end) {
        if (end >= start) {
            return end - start;
        }
        // handle overflow
        return end + ~start + 1;
    }

    void _newTransmission() {
        uint32_t diff = __inline_get_time_diff(_startMillis, millis());
        __LDBG_printf("last transmission time=%ld", (long)diff);
        _startMillis = millis();
    }
#else
    inline void _newTransmission() {}
#endif

#if I2C_OVER_UART_ADD_CRC16
    size_t _printHexCrc(uint16_t crc);
    size_t _printHexUpdateCrc(uint8_t data, uint16_t = flags()._crc);
#endif

protected:
    Data_t &data();
    Data_t &flags();
    void _invokeOnReceive(int len);
    void _invokeOnRequest();
    void _invokeOnReadSerial();

protected:
    Data_t _data;
    uint8_t _buffer[kCommandBufferSize];

    SerialTwoWireStream _in;            // incoming messages
    SerialTwoWireStream _out;           // output buffer for write

    onReceiveCallback _onReceive;
    onRequestCallback _onRequest;
    onReadSerialCallback _onReadSerial;

    Stream *_serial;
};

#include "SerialTwoWireSlave.hpp"

#if HAVE_KFC_FIRMWARE_VERSION
#include <debug_helper_disable.h>
#endif
