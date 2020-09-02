/**
 * Author: sascha_lammers@gmx.de
 */

#include "SerialTwoWire.h"
#include "SerialTwoWireSlave.h"

#if DEBUG_SERIALTWOWIRE
#include <debug_helper.h>
#include <debug_helper_enable.h>
#undef __LDBG_assert
#define __LDBG_assert(...) assert(__VA_ARGS__)
#else
#undef __LDBG_printf
#undef __LDBG_print
#undef __LDBG_println
#undef __LDBG_assert
#define __LDBG_printf(...)
#define __LDBG_print(...)
#define __LDBG_println(...)
#define __LDBG_assert(...)
#endif

SerialTwoWireSlave::SerialTwoWireSlave() : SerialTwoWireSlave(Serial)
{
}

SerialTwoWireSlave::SerialTwoWireSlave(Stream &serial, onReadSerialCallback callback) :
    _address(0),
    _length(0),
    _command(NONE),
    _buffer{},
    _serial(serial),
    _onReadSerial(callback)
{
}

SerialTwoWireSlave::SerialTwoWireSlave(Stream &serial) :
    _address(0),
    _length(0),
    _command(NONE),
    _buffer{},
    _serial(serial)
#ifndef SERIALTWOWIRE_NO_GLOBALS
    , _onReadSerial(serialEvent)
#endif
{
}

void SerialTwoWireSlave::end()
{
    _address = 0;
    _length = 0;
    _command = NONE;
    _in.release();
    _out.release();
}

void SerialTwoWireSlave::beginTransmission(uint8_t address)
{
    __LDBG_printf("address=0x%02x", address);
    _out.clear();
    _out.write(address);
}

uint8_t SerialTwoWireSlave::endTransmission(uint8_t stop)
{
    _serial.print(reinterpret_cast<const __FlashStringHelper *>(_i2c_transmit_cmd));
    while (_out.available()) {
        _printHex(_out.read());
    }
    _out.clear();
    _serial.println();
    return 0;
}


size_t SerialTwoWireSlave::write(uint8_t data)
{
    return _out.write(data);
}

size_t SerialTwoWireSlave::write(const uint8_t *data, size_t length)
{
    return _write(data, length);
}

int SerialTwoWireSlave::available()
{
    return _in.available();
}

int SerialTwoWireSlave::read()
{
    auto data = _in.read();
    if (available() == 0) {
        _in.clear();
    }
    return data;
}

int SerialTwoWireSlave::peek()
{
    return _in.peek();
}

size_t SerialTwoWireSlave::readBytes(uint8_t *buffer, size_t length)
{
    return _in.readBytes(buffer, length);
}

size_t SerialTwoWireSlave::readBytes(char *buffer, size_t length)
{
    return _in.readBytes(reinterpret_cast<uint8_t *>(buffer), length);
}

void SerialTwoWireSlave::_newLine()
{
    __LDBG_printf("cmd=%u length=%u _in=%u", _command, _length, _in.length());
    if (_length == 1 || _command <= STOP_LINE) {
        // invalid data, discard
        if (_in.length()) {
            _in.clear();
        }
    }
    else if (_in.length()) {
        if (_length) { // 2 byte left
            _addBuffer(_parseData());
        }
        _processData();
    }
    _command = NONE;
    _length = 0;
}

void SerialTwoWireSlave::feed(uint8_t data)
{
    if (data == '\n') { // check first
        _newLine();
    }
    else if (_command == STOP_LINE || data == '\r') {
        // skip rest of the line cause of invalid data
    }
    else if (_command == NONE) {
        if (_length == 0 && data != '+') {
            // must start with +
            __LDBG_printf("discard data=%u", data);
            _discard();
        }
        else {
            __LDBG_assert(_length < sizeof(_buffer) - 1);
            // append
            _buffer[_length++] = data;
            _buffer[_length] = 0;
            if (strcasecmp_P(reinterpret_cast<const char *>(_buffer), _i2c_transmit_cmd) == 0) {
                _command = TRANSMIT;
                _length = 0;
            }
            else if (_length >= sizeof(_buffer) - 1) { // buffer full
                __LDBG_printf("discard length=%u", _length);
                _discard();
            }
        }
    }
    else if (isxdigit(data)) {
        __LDBG_assert(_length < sizeof(_buffer) - 1);
        __LDBG_assert(_length < 2);
        // add data to command buffer
        _buffer[_length++] = data;
        if (_length == 2) {
            _addBuffer(_parseData());
        }
    }
    else if (data != ',' && !isspace(data)) {
        // invalid data, discard
        __LDBG_printf("discard data=%u", data);
        _discard();
    }
}

uint8_t SerialTwoWireSlave::_parseData()
{
    __LDBG_assert(_length == 2);
    _buffer[_length] = 0;
    return (uint8_t)strtoul(reinterpret_cast<const char *>(_buffer), nullptr, 16);
}

void SerialTwoWireSlave::_addBuffer(uint8_t data)
{
    if (_in.length()) {
        // write to _in
        _in.write(data);
    }
    else {
        __LDBG_assert(_in.length() == 0);
        if (data == _address) {
            // add address to buffer to indicate its use
            _in.write(data);
        }
        else {
            // invalid address, discard
            __LDBG_printf("address=0x%02x _address=0x%02x", data, _address);
            _discard();
        }
    }
    _length = 0;
}

void SerialTwoWireSlave::_processData()
{
    if (_in.length() == 1) {
        // no data, discard
        _in.clear();
    }
    if (_command == TRANSMIT) {
        if (_in.length()) {
#if DEBUG_SERIALTWOWIRE
            auto address =
#endif
            _in.read();
            __LDBG_printf("available=%u addr=0x%02x _addr=0x%02x", _in.available(), address, _address);
            // address was already checked
            _onReceive(_in.available());
            _in.clear();
        }
    }
}

void SerialTwoWireSlave::_serialEvent()
{
    while (_serial.available()) {
        feed(_serial.read());
    }
}
