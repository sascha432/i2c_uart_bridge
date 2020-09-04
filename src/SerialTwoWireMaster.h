/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "SerialTwoWireSlave.h"

class SerialTwoWireMaster : public SerialTwoWireSlave
{
public:
    using SerialTwoWireSlave::SerialTwoWireSlave;
    using Stream::setTimeout;

    //inline void setTimeout(unsigned long _timeout);

    void begin();
    using SerialTwoWireSlave::begin;

    uint8_t requestFrom(uint8_t address, uint8_t count, uint8_t stop = true);
    inline uint8_t requestFrom(int address, int count, int stop = true) {
        return requestFrom((uint8_t)address, (uint8_t)count, (uint8_t)stop);
    }

    virtual int available() override;
    virtual int read() override;
    virtual int peek() override;
    virtual size_t readBytes(uint8_t *buffer, size_t length) override;
    virtual size_t readBytes(char *buffer, size_t length) override;

    virtual void feed(uint8_t data);

protected:
    void _newLine();
    void _addBuffer(int data);
    void _processData();
    uint8_t _waitForResponse(uint8_t address, uint8_t count);

protected:
    // SerialTwoWireStream _request;       // incoming message for requestFrom
    //SerialTwoWireStream &_request;
    //SerialTwoWireStream *_read;         // points to the message buffer either master or slave
};
