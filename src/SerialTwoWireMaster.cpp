/**
 * Author: sascha_lammers@gmx.de
 */

#include "SerialTwoWireMaster.h"

SerialTwoWireMaster::SerialTwoWireMaster() :
    _read(&_request)
{
}

SerialTwoWireMaster::SerialTwoWireMaster(Stream &serial) :
    SerialTwoWireSlave(serial),
    _read(&_request)
{
}

SerialTwoWireMaster::SerialTwoWireMaster(Stream &serial, onReadSerialCallback callback) :
    SerialTwoWireSlave(serial, callback),
    _read(&_request)
{
}

void SerialTwoWireMaster::end()
{
    SerialTwoWireSlave::end();
    _request.release();
    _read = &_request;
}

uint8_t SerialTwoWireMaster::requestFrom(uint8_t address, uint8_t count, uint8_t stop)
{
    __LDBG_printf("addr=0x%02x count=%u stop=%u len=%u", address, count, stop, _request.length());

    if (count == 0 || address == 0) {
        return 0;
    }

    // discard any data from previous requests
    _request.clear();
    _request.write(address);

    // send request
    _serial.print(reinterpret_cast<const __FlashStringHelper *>(_i2c_request_cmd));
    _printHex(address);
    _printHex(count);
    _serial.println();

    // wait response
    return _waitForResponse(address, count);
}

uint8_t SerialTwoWireMaster::_waitForResponse(uint8_t address, uint8_t count)
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

int SerialTwoWireMaster::available()
{
    return _read->available();
}

int SerialTwoWireMaster::read()
{
    auto data = _read->read();
    if (available() == 0) {
        _read->clear();
    }
    return data;
}

int SerialTwoWireMaster::peek()
{
    return _read->peek();
}

size_t SerialTwoWireMaster::readBytes(uint8_t *buffer, size_t length)
{
    return _read->readBytes(buffer, length);
}

size_t SerialTwoWireMaster::readBytes(char *buffer, size_t length)
{
    return _read->readBytes(reinterpret_cast<uint8_t *>(buffer), length);
}

void SerialTwoWireMaster::_newLine()
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
            _addBuffer(_parseData());
        }
        _processData();
    }
    _command = NONE;
    _length = 0;
}

void SerialTwoWireMaster::_addBuffer(uint8_t data)
{
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
            _discard();
        }
    }
    _length = 0;
}

void SerialTwoWireMaster::_processData()
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
#if DEBUG_SERIALTWOWIRE
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
            __LDBG_printf("available=%u ", _request.available());
        }
    }
}

void SerialTwoWireMaster::feed(uint8_t data)
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
            else if (strcasecmp_P(reinterpret_cast<const char *>(_buffer), _i2c_request_cmd) == 0) {
                _command = REQUEST;
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
