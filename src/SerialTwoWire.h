/**
 * Author: sascha_lammers@gmx.de
 */

// I2C over UART emulation

#pragma once

#include <Arduino.h>
#if SERIALTWOWIRE_USE_BUFFERSTREAM
#include <BufferStream.h>
#else
#include <StreamString.h>
#endif

class Serial;

class SerialTwoWire : public Stream {
public:
    typedef void (* onReceiveCallback)(int);
    typedef void (* onRequestCallback)();
    typedef void (* onReadSerialCallback)();

    typedef enum {
        NONE = 0,
        TRANSMIT,
        REQUEST,
    } CommandEnum_t;    

    SerialTwoWire();
    SerialTwoWire(Stream &serial);
    void setSerial(Stream &serial);

    void begin(uint8_t address) {
        end();
        _address = address;
    }
    inline void begin(int address) {
        begin((uint8_t)address);
    }

    void begin() {
        end();
    }
    void end() {
        _pos = 0;
        _command = NONE;
        _address = 0;
        _in.clear();
        _out.clear();
    }

    void setClock(uint32_t) {
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
    virtual void flush(void) {
    }

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
    void _printHex(uint8_t data, Stream &stream);

    uint8_t _address;
    uint8_t _recvAddress;
    bool externalFeed;
#if SERIALTWOWIRE_USE_BUFFERSTREAM
    BufferStream _in;
    BufferStream _out;
#else
    StreamString _in;
    StreamString _out;
#endif
    uint8_t _pos;
    CommandEnum_t _command;
    char _buffer[8];
    Stream &_serial;

    onReceiveCallback _onReceive;
    onRequestCallback _onRequest;
    onReadSerialCallback _onReadSerial;
};

#ifndef SERIALTWOWIRE_NO_GLOBALS
extern SerialTwoWire Wire;
#endif
