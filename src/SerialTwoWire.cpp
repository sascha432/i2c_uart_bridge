/**
 * Author: sascha_lammers@gmx.de
 */

#include "SerialTwoWire.h"

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

#ifndef SERIALTWOWIRE_NO_GLOBALS

SerialTwoWire Wire;

void serialEvent()
{
    Wire._serialEvent();
}

#endif

static const char _i2c_transmit_cmd[] PROGMEM = { I2C_OVER_UART_PREFIX_TRANSMIT };
static const char _i2c_request_cmd[] PROGMEM = { I2C_OVER_UART_PREFIX_REQUEST };

SerialTwoWire::SerialTwoWire() : SerialTwoWire(Serial)
{
}

SerialTwoWire::SerialTwoWire(Stream &serial, onReadSerialCallback callback) :
    _address(0),
    _length(0),
    _command(NONE),
    _buffer{},
    _read(&_request),
    _serial(serial),
    _onReadSerial(callback)
{
}

SerialTwoWire::SerialTwoWire(Stream &serial) :
    _address(0),
    _length(0),
    _command(NONE),
    _buffer{},
    _read(&_request),
    _serial(serial)
#ifndef SERIALTWOWIRE_NO_GLOBALS
    , _onReadSerial(serialEvent)
#endif
{
}

void SerialTwoWire::end()
{
    _address = 0;
    _length = 0;
    _command = NONE;
    _in.clear();
    _request.clear();
    _out.clear();
    _read = &_request;
}

void SerialTwoWire::beginTransmission(uint8_t address)
{
    __LDBG_printf("address=0x%02x", address);
    _out.clear();
    _out.write(address);
}

uint8_t SerialTwoWire::endTransmission(uint8_t stop)
{
    _serial.print(reinterpret_cast<const __FlashStringHelper *>(_i2c_transmit_cmd));
    while (_out.available()) {
        _printHex(_out.read());
    }
    _out.clear();
    _serial.println();
    return 0;
}

uint8_t SerialTwoWire::requestFrom(uint8_t address, uint8_t count, uint8_t stop)
{
    __LDBG_printf("addr=0x%02x count=%u stop=%u len=%u", address, count, stop, _request.length());

    if (count == 0 || address == 0) {
        return 0;
    }

    // discard any data from previous requests
    _request.removeAndShrink(0);
    _request.write(address);

    // send request
    _serial.print(reinterpret_cast<const __FlashStringHelper *>(_i2c_request_cmd));
    _printHex(address);
    _printHex(count);
    _serial.println();

    // wait response
    return _waitForResponse(address, count);
}

uint8_t SerialTwoWire::_waitForResponse(uint8_t address, uint8_t count)
{
    unsigned long timeout = millis() + _timeout;
    while(_request[0] != kFinishedRequest && millis() <= timeout) {
#if defined(ESP8266) || defined(ESP32)
            optimistic_yield(1000);
#endif
        if (_onReadSerial) {
            _onReadSerial();
        }
    }
    __LDBG_printf("peek=0x%02x count=%u available=%u", _request.peek(), count, _request.available());
    if (_request.read() == kFinishedRequest && _request.available() == count) {
        // address was removed = done
        return count;
     }
     // timeout, wrong address, wrong size...
     _request.clear();
    return 0;
}

size_t SerialTwoWire::write(uint8_t data)
{
    return _out.write(data);
}

size_t SerialTwoWire::write(const uint8_t *data, size_t length)
{
    size_t count = length;
    while (count--) {
        _out.write(*data++);
    }
    return length;
}

int SerialTwoWire::available(void)
{
    return _read->available();
}

int SerialTwoWire::read(void)
{
    auto data = _read->read();
    if (_read->length() == _read->position() && _read->length()) {
        // free memory when reaching end of stream
        _read->clear();
    }
    return data;
}

int SerialTwoWire::peek(void)
{
    return _read->peek();
}

size_t SerialTwoWire::readBytes(uint8_t *buffer, size_t length)
{
    return _read->readBytes(buffer, length);
}

size_t SerialTwoWire::readBytes(char *buffer, size_t length)
{
    return _read->readBytes(reinterpret_cast<uint8_t *>(buffer), length);
}

void SerialTwoWire::newLine()
{
    __LDBG_printf("cmd=%u length=%u _in=%u _request=%u", _command, _length, _in.length(), _request.charAt(0) == kFillingRequest ? _request.length() : 0);
    if (_length == 1 || _command <= STOP_LINE) {
        // invalid data, discard
        if (_in.length()) {
            _in.clear();
        }
        else if (_request.charAt(0) == kFillingRequest) {
            _request.clear();
        }
    }
    else if (_in.length() || _request.charAt(0) == kFillingRequest) {
        if (_length) { // 2 byte left
            _addBuffer();
        }
        _processData();
    }
#if SERIALTWOWIRE_DEBUG
    else {
        Serial.print(F("newLine=cmd=NONE,"));
        Serial.println(_buffer);
    }
#endif
    _command = NONE;
    _length = 0;
}

void SerialTwoWire::feed(uint8_t data)
{
    if (data == '\n') { // check first
        newLine();
    }
    else if (_command == STOP_LINE || data == '\r') {
        // skip rest of the line cause of invalid data
    }
    else if (_command == NONE) {
        if (_length == 0 && data != '+') {
            // must start with +
            __LDBG_printf("discard data=%u", data);
            stopLine();
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
            else if (strcasecmp_P(reinterpret_cast<const char *>(_buffer), _i2c_request_cmd) == 0) {
                _command = REQUEST;
                _length = 0;
            }
            else if (_length >= sizeof(_buffer) - 1) { // buffer full
                __LDBG_printf("discard length=%u", _length);
                stopLine();
            }
        }
    }
    else if (isxdigit(data)) {
        __LDBG_assert(_length < sizeof(_buffer) - 1);
        __LDBG_assert(_length < 2);
        // add data to command buffer
        _buffer[_length++] = data;
        if (_length == 2) {
            _addBuffer();
        }
    }
    else if (data != ',' && !isspace(data)) {
        // invalid data, discard
        __LDBG_printf("discard data=%u", data);
        stopLine();
    }
}

void SerialTwoWire::_addBuffer()
{
    __LDBG_assert(_length == 2);

    _buffer[_length] = 0;
    auto data = (uint8_t)strtoul(reinterpret_cast<const char *>(_buffer), nullptr, 16);

    if (_request.charAt(0) == kFillingRequest) {
        _request.write(data);
    }
    else if (_in.length()) {
        // write to _in
        if (_command == REQUEST && _in.length() >= kRequestCommandMaxLength) {
            return; // we only need 2 bytes for a request
        }
        _in.write(data);
    }
    else {
        __LDBG_assert(_in.length() == 0);
        if (data == _address) {
            // add address to buffer to indicate its use
            _in.write(data);
        }
        else if (_request.length() && _request.charAt(0) == data) {
            // mark as being processed
            _request[0] = kFillingRequest;
        }
        else {
            // invalid address, discard
            __LDBG_printf("address=0x%02x _address=0x%02x _request=0x%02x", data, _address, _request.charAt(0) & 0xffff);
            stopLine();
        }
    }
}

void SerialTwoWire::_processData()
{
    if (_in.length() == 1) {
        // no data, discard
        _in.clear();
    }
    else if (_command == REQUEST) {
        // request has address and length only
        uint8_t len = _in.charAt(1);
        __LDBG_printf("requestFrom slave=0x%02x len=%u", _in.charAt(0), len);
        beginTransmission((uint8_t)_address);
        _onRequest();
        while (_out.length() <= len) {
            _out.write(kReadValueOutOfRange);
        }
        endTransmission();
        _in.clear();
    }
    else if (_command == TRANSMIT) {
        if (_in.length()) {
#if SERIALTWOWIRE_DEBUG
            auto address =
#endif
            _in.read();
            __LDBG_printf("available=%u addr=0x%02x _addr=0x%02x", _in.available(), address, _address);
            // address was already checked
            _read = &_in;
            _onReceive(_in.available());
            _read = &_request;
            _in.clear();
        }
        else if (_request.charAt(0) == kFillingRequest) {
            // mark as finished
            _request[0] = kFinishedRequest;
        }
    }
}

void SerialTwoWire::_serialEvent()
{
    while (_serial.available()) {
        feed(_serial.read());
    }
}
