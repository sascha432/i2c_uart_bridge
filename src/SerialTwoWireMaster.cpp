/**
 * Author: sascha_lammers@gmx.de
 */

#include "SerialTwoWire.h"
#include "SerialTwoWireMaster.h"
#include "SerialTwoWireDebug.h"

#include <debug_helper_enable.h>

void SerialTwoWireMaster::begin()
{
    data()._address = 0;
}

uint8_t SerialTwoWireMaster::requestFrom(uint8_t address, uint8_t count, uint8_t stop)
{
    __LDBG_printf("addr=%02x count=%u stop=%u len=%u out_state=%u", address, count, stop, _request().length(), flags()._outState);

    if (count == 0 || address == 0) {
        return 0;
    }

    flags()._setOutState(OutStateType::FILL);
    // discard any data from previous requests
    _request().clear();
    _request().write(address);

    // send request
#if I2C_OVER_UART_ADD_CRC16
    auto crc = _crc16_update(_crc16_update(~0, address), count);
#endif
    auto ptr = requestCommand;
    uint8_t data;
    // write as fast as possible
    _serial->flush();
    while((data = pgm_read_byte(ptr++)) != 0) {
        _serial->write(data);
    }
    _printHex(address);
    _printHex(count);
#if I2C_OVER_UART_ADD_CRC16
    _printHexCrc(crc);
#else
    _println();
#endif
    _serial->flush();

    // wait for response
#if DEBUG_SERIALTWOWIRE
    auto start = micros();
    auto result = _waitForResponse(address, count);
    auto dur = micros() - start;
    __DBG_printf("response=%d time=%uus can_yield=%u out_state=%u", result, dur, can_yield(), flags()._outState);
    return result;
#else
    return _waitForResponse(address, count);
#endif
}

#include <PrintString.h>

uint8_t SerialTwoWireMaster::_waitForResponse(uint8_t address, uint8_t count)
{
    __LDBG_assert_printf((bool)_onReceive, "onReadSerial=%u:%p", (bool)_onReadSerial, &_onReadSerial);
    unsigned long timeout = millis() + _timeout;
    while(flags()._outIsFilling() && millis() <= timeout) {
        optimistic_yield(1000);
        if (_onReadSerial) {
            _onReadSerial();
        }
    }
    __LDBG_printf("count=%u _ravail=%u _rlen=%u out_state=%u", _request().charAt(0), _request().available(), _request().length(), flags()._outState);
    if (flags()._getOutState() == OutStateType::FILLED && _request().read() == address && _request().available() == count) {
        // address was removed = done
        flags()._setOutState(OutStateType::NONE);
        return count;
     }
     //PrintString str;
     //size_t len = _request().length();
     //size_t avail = _request().available();
     //while  (_request().available()) {
     //   str.printf_P(PSTR("%02x "), _request().read());
     //}
     //__LDBG_printf("len=%u avail=%u data=%s", len, avail, str.c_str());
     // timeout, wrong address, wrong size...
    _request().clear();
    flags()._setOutState(OutStateType::NONE);
    return 0;
}

int SerialTwoWireMaster::available()
{
    return readFrom().available();
}

int SerialTwoWireMaster::read()
{
    auto &read = readFrom();
    auto data = read.read();
    if (read.available() == 0) { // resize when empty
        read.clear();
    }
    return data;
}

int SerialTwoWireMaster::peek()
{
    return readFrom().peek();
}

size_t SerialTwoWireMaster::readBytes(uint8_t *buffer, size_t length)
{
    return readFrom().readBytes(buffer, length);
}

size_t SerialTwoWireMaster::readBytes(char *buffer, size_t length)
{
    return readFrom().readBytes(reinterpret_cast<uint8_t *>(buffer), length);
}

void SerialTwoWireMaster::_newLine()
{
    // add any data that's left in the buffer
    _addBuffer(_parseData(true));

    __LDBG_printf("cmd=%u length=%u _in=%u _request avail=%u discard=%u out_state=%u",
        flags()._command, data()._length, _in.length(),
        (flags()._getOutState() == OutStateType::FILLING ? _request().available() : 0),
        (flags()._command <= DISCARD || (_in.length() == 0 && flags()._outIsFilling() == false)),
        flags()._outState
    );

    if (flags()._command <= DISCARD || (_in.length() == 0 && flags()._outIsFilling() == false)) {
        if (_in.length()) {
            _in.clear();
        }
        else if (flags()._outIsFilling() == false) {
            _request().clear();
        }
    }
    else {
        _processData();
    }
    flags()._command = NONE;
    data()._length = 0;
#if I2C_OVER_UART_ADD_CRC16
    flags()._crcMarker = false;
    flags()._crc = ~0;
#endif
}

void SerialTwoWireMaster::_addBuffer(int byte)
{
    if (byte == kNoDataAvailable) {
        return;
    }
    //__LDBG_printf("data=%02x _addr=%02x _request=%02x out_state=%u _ravail=%u _rlen=%u", byte, data()._address, _request().charAt(0) & 0xffff, flags()._outState, _request().available(), _request().length());
    if (flags()._getOutState() == OutStateType::FILLING) {
        if (_request().length() >= kTransmissionMaxLength) {
            __LDBG_printf("data=%d _request=%u max=%u", byte, _request().length(), kTransmissionMaxLength);
            _discard();
        }
        else {
            // __LDBG_printf("data=%u out_state=%u avail=%u len=%u", byte, flags()._outState, _request().available(), _request().length());
            _request().write(byte);
        }
    }
    else if (_in.length()) {
        // write to _in
        if (flags()._command == REQUEST && _in.length() >= kRequestTransmissionMaxLength) {
            // we only need 2 bytes for a request
        }
        else if (_in.length() >= kTransmissionMaxLength) {
            __LDBG_printf("data=%d _in=%u max=%u", byte, _in.length(), kTransmissionMaxLength);
            _discard();
        }
        else {
            _in.write(byte);
        }
    }
    else {
        __LDBG_assert_printf(_in.length() == 0, "length=%u data=%d", data()._length, byte);
        if (byte == data()._address) {
            // add address to buffer to indicate its use
            _in.write(byte);
        }
        else if (_request().length() == 1 && flags()._getOutState() == OutStateType::FILL && _request()[0] == byte) {
            // mark as being processed
            flags()._setOutState(OutStateType::FILLING);
            //// remove address that has been added by requestFrom()
            //_request().pop_back();
            __LDBG_printf("addr=%02x out_state=%u avail=%u len=%u", _request()[0], flags()._outState, _request().available(), _request().length());
        }
        else {
            // invalid address, discard
            __LDBG_printf("addr=%02x _addr=%02x _request=%02x out_state=%u", byte, data()._address, _request().charAt(0) & 0xffff, flags()._outState);
            _discard();
        }
    }
    data()._length = 0;
}

void SerialTwoWireMaster::_processData()
{
    if (_in.length() == 1) {
        __DBG_assert_printf(_in.length() == _in.available(), "len=%u available=%u", _in.length(), _in.available());
        // no data, discard
        _in.clear();
    }
    else if (flags()._command == REQUEST) {
        // request has address and length only
        uint8_t len = _in.charAt(1);
        __LDBG_printf("requestFrom slave=%02x master=%02x len=%u", _in.charAt(0), data()._address, len);
        beginTransmission(data()._address);
        // point to empty stream
        flags()._readFromOut = false;
        __LDBG_assert_printf((bool)_onRequest, "onRequest=%u:%p", (bool)_onRequest, &_onRequest);
        _onRequest();
        flags()._readFromOut = true;
        while (_out.length() <= len) {
            _out.write(kReadValueOutOfRange);
        }
        _endTransmission(true);
    }
    else if (flags()._command == TRANSMIT) {
        if (_in.length()) {
            __DBG_assert_printf(_in.length() == _in.available(), "len=%u available=%u", _in.length(), _in.available());
            __LDBG_printf("available=%u addr=%02x _addr=%02x", _in.available(), _in.peek(), data()._address);
            // address has been checked already
            _in.read();
            flags()._readFromOut = false;
            __LDBG_assert_printf((bool)_onReceive, "onReceive=%u:%p", (bool)_onReceive, &_onReceive);
            _onReceive(_in.available());
            flags()._readFromOut = true;
            _in.clear();
        }
        else if (flags()._getOutState() == OutStateType::FILLING) {
            __DBG_assert_printf(_request().length() == _request().available(), "len=%u available=%u", _request().length(), _request().available());
            // mark as finished
            flags()._setOutState(OutStateType::FILLED);
            __LDBG_printf("addr=%02x available=%u out_state=%u", _request().peek(), _request().available(), flags()._outState);
        }
    }
}

void SerialTwoWireMaster::feed(uint8_t byte)
{
    if (byte == '\n') { // check first
        _newLine();
    }
    else if (flags()._command == DISCARD || byte == '\r') {
        // skip rest of the line cause of invalid data
    }
    else if (flags()._command == NONE) {
        static_assert(kMaxTransmissionLength < sizeof(_buffer), "invalid size");
        if ((data()._length == 0 && byte != '+') || data()._length >= kMaxTransmissionLength) {
            // must start with +
            __LDBG_printf("discard data=%u length=%u", byte, data()._length);
            _discard();
        }
        else {
            // append
            _buffer[data()._length++] = byte;
            _buffer[data()._length] = 0;
            if (strcasecmp_P(reinterpret_cast<const char *>(_buffer), transmitCommand) == 0) {
                flags()._command = TRANSMIT;
                data()._length = 0;
            }
            else if (strcasecmp_P(reinterpret_cast<const char *>(_buffer), requestCommand) == 0) {
                flags()._command = REQUEST;
                data()._length = 0;
            }
        }
    }
#if I2C_OVER_UART_ADD_CRC16
    else if (byte == kCrcStartChar && !flags()._crcMarker) {
        flags()._crcMarker = true;
    }
#endif
    else if (isxdigit(byte)) {
        __LDBG_assert_printf(data()._length < sizeof(_buffer) - 1, "buf=%-*.*s", (sizeof(_buffer) - 1), (sizeof(_buffer) - 1), _buffer);
        // add data to command buffer
        _buffer[data()._length++] = byte;
        _addBuffer(_parseData());
    }
    else if (byte != ',' && !isspace(byte)) {
        // invalid data, discard
        __LDBG_printf("discard data=%u", byte);
        _discard();
        data()._length = 0;
    }
}
