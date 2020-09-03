/**
 * Author: sascha_lammers@gmx.de
 */

#include "SerialTwoWire.h"
#include "SerialTwoWireMaster.h"
#include "SerialTwoWireDebug.h"

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
    __LDBG_printf("addr=%02x count=%u stop=%u len=%u", address, count, stop, _request.length());

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
#if I2C_OVER_UART_ADD_CRC16
    _serial.print(I2C_OVER_UART_CRC_START);
    auto crc = _crc16_update(_crc16_update(~0, address), count);
    _printHex((uint8_t)crc);
    _printHex(crc >> 8);
#endif
    _serial.println();

    // wait for response
#if DEBUG_SERIALTWOWIRE
    auto start = micros();
    auto result = _waitForResponse(address, count);
    auto dur = micros() - start;
    __DBG_printf("response=%d time=%uus can_yield=%u", result, dur, can_yield());
    return result;
#else
    return _waitForResponse(address, count);
#endif
}

uint8_t SerialTwoWireMaster::_waitForResponse(uint8_t address, uint8_t count)
{
    unsigned long timeout = millis() + _timeout;
    while(_request[0] != kFinishedRequest && millis() <= timeout) {
        optimistic_yield(1000);
        if (_onReadSerial) {
            _onReadSerial();
        }
    }
    __LDBG_printf("peek=%02x count=%u available=%u", _request.peek(), count, _request.available());
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
    if (_read->available() == 0) { // resize when empty
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
    // add any data that's left in the buffer
    _addBuffer(_parseData(true));

#if DEBUG_BUFFER_ALLOC && DEBUG_SERIALTWOWIRE
    size_t _inLen = _in.length();
    size_t _requestLen = _request.length();
#endif

    __LDBG_printf("cmd=%u length=%u _in=%u _request=%u discard=%u",
        _command, _length, _in.length(),
        (_request.charAt(0) == kFillingRequest ? _request.length() : 0),
        (_command <= DISCARD || (_in.length() == 0 && _request.charAt(0) != kFillingRequest))
    );

    if (_command <= DISCARD || (_in.length() == 0 && _request.charAt(0) != kFillingRequest)) {
        if (_in.length()) {
            _in.clear();
        }
        else if (_request.charAt(0) == kFillingRequest) {
            _request.clear();
        }
    }
    else {
        _processData();
    }
    _command = NONE;
    _length = 0;
#if I2C_OVER_UART_ADD_CRC16
    _crcMarker = false;
    _crc = ~0;
#endif

#if DEBUG_BUFFER_ALLOC && DEBUG_SERIALTWOWIRE
    _inLen = std::max(_in.length(), _inLen);
    _requestLen = std::max(_request.length(), _requestLen);
    Serial.printf_P(PSTR("SerialTwoWire %lu: transmissions=%u _in=%u "), (unsigned long)millis(), _transmissions, _inLen);
    _in.dumpAlloc(Serial);
    Serial.printf_P(PSTR(" _request=%u "), _requestLen);
    _request.dumpAlloc(Serial);
    Serial.println();
#endif
}

void SerialTwoWireMaster::_addBuffer(int data)
{
    if (data == kNoDataAvailable) {
        return;
    }
    if (_request.charAt(0) == kFillingRequest) {
        if (_request.length() >= kTransmissionMaxLength) {
            _discard();
        }
        else {
            _request.write(data);
        }
    }
    else if (_in.length()) {
        // write to _in
        if (_command == REQUEST && _in.length() >= kRequestCommandMaxLength) {
            // we only need 2 bytes for a request
        }
        else if (_in.length() >= kTransmissionMaxLength) {
            _discard();
        }
        else {
            _in.write(data);
        }
    }
    else {
        __LDBG_assert_printf(_in.length() == 0, "length=%u data=%d", _length, data);
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
            __LDBG_printf("address=%02x _address=%02x _request=%02x", data, _address, _request.charAt(0) & 0xffff);
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
        __LDBG_printf("requestFrom slave=%02x len=%u", _in.charAt(0), len);
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
            __LDBG_printf("available=%u addr=%02x _addr=%02x", _in.available(), address, _address);
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
    else if (_command == DISCARD || data == '\r') {
        // skip rest of the line cause of invalid data
    }
    else if (_command == NONE) {
        static_assert(kSerialTwoWireMaxCommandLength < sizeof(_buffer), "invalid size");
        if ((_length == 0 && data != '+') || _length >= kSerialTwoWireMaxCommandLength) {
            // must start with +
            __LDBG_printf("discard data=%u length=_length", data, _length);
            _discard();
        }
        else {
            // append
            _buffer[_length++] = data;
            _buffer[_length] = 0;
            if (strcasecmp_P(reinterpret_cast<const char *>(_buffer), _i2c_transmit_cmd) == 0) {
                _command = TRANSMIT;
                _length = 0;
#if DEBUG_SERIALTWOWIRE && DEBUG_BUFFER_ALLOC
                _transmissions++;
#endif
            }
            else if (strcasecmp_P(reinterpret_cast<const char *>(_buffer), _i2c_request_cmd) == 0) {
                _command = REQUEST;
                _length = 0;
#if DEBUG_SERIALTWOWIRE && DEBUG_BUFFER_ALLOC
                _transmissions++;
#endif
            }
        }
    }
#if I2C_OVER_UART_ADD_CRC16
    else if (data == I2C_OVER_UART_CRC_START && !_crcMarker) {
        _crcMarker = true;
    }
#endif
    else if (isxdigit(data)) {
        __LDBG_assert_printf(_length < sizeof(_buffer) - 1, "buf=%-*.*s", (sizeof(_buffer) - 1), (sizeof(_buffer) - 1), _buffer);
        // add data to command buffer
        _buffer[_length++] = data;
        _addBuffer(_parseData());
    }
    else if (data != ',' && !isspace(data)) {
        // invalid data, discard
        __LDBG_printf("discard data=%u", data);
        _discard();
        _length = 0;
    }
}
