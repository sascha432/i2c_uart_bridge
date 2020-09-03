/**
 * Author: sascha_lammers@gmx.de
 */

#include "SerialTwoWire.h"
#include "SerialTwoWireSlave.h"
#include "SerialTwoWireDebug.h"

SerialTwoWireSlave::SerialTwoWireSlave() : SerialTwoWireSlave(Serial)
{
}

SerialTwoWireSlave::SerialTwoWireSlave(Stream &serial, onReadSerialCallback callback) :
    _address(0),
    _length(0),
    _command(NONE),
#if I2C_OVER_UART_ADD_CRC16
    _crcMarker(false),
    _crc(~0),
#endif
    _buffer{},
    _onReadSerial(callback),
#if DEBUG_SERIALTWOWIRE && DEBUG_BUFFER_ALLOC
    _transmissions(0),
#endif
    _serial(serial)
{
}

SerialTwoWireSlave::SerialTwoWireSlave(Stream &serial) :
    _address(0),
    _length(0),
    _command(NONE),
#if I2C_OVER_UART_ADD_CRC16
    _crcMarker(false),
    _crc(~0),
#endif
    _buffer{},
#ifndef SERIALTWOWIRE_NO_GLOBALS
    _onReadSerial(serialEvent),
#endif
#if DEBUG_SERIALTWOWIRE && DEBUG_BUFFER_ALLOC
    _transmissions(0),
#endif
    _serial(serial)
{
}

void SerialTwoWireSlave::end()
{
    _address = 0;
    _length = 0;
    _command = NONE;
#if I2C_OVER_UART_ADD_CRC16
    _crcMarker = false;
    _crc = ~0;
#endif
    _in.release();
    _out.release();
}

void SerialTwoWireSlave::beginTransmission(uint8_t address)
{
    __LDBG_printf("addr=%02x", address);
    _out.clear();
    _out.write(address);
}

uint8_t SerialTwoWireSlave::endTransmission(uint8_t stop)
{
    _serial.print(reinterpret_cast<const __FlashStringHelper *>(_i2c_transmit_cmd));
#if I2C_OVER_UART_ADD_CRC16
    uint16_t crc = ~0;
    while (_out.available()) {
        auto data = _out.read();
        crc = _crc16_update(crc, data);
        _printHex(data);
    }
    _serial.print(I2C_OVER_UART_CRC_START);
    _printHex(crc >> 8);
    _printHex((uint8_t)crc);
#else
    while (_out.available()) {
        _printHex(_out.read());
    }
#endif
#if DEBUG_BUFFER_ALLOC && DEBUG_SERIALTWOWIRE
    size_t len = _out.length();
#endif
    _out.clear();
    _serial.println();
#if DEBUG_BUFFER_ALLOC && DEBUG_SERIALTWOWIRE
    Serial.printf_P(PSTR("SerialTwoWire %lu: transmissions=%u _out=%u "), (unsigned long)millis(), ++_transmissions, len);
    _in.dumpAlloc(Serial);
    Serial.println();
#endif

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
    if (_in.available() == 0) { // resize when empty
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
    // add any data left in the buffer
    _addBuffer(_parseData(true));

#if DEBUG_BUFFER_ALLOC && DEBUG_SERIALTWOWIRE
    size_t _inLen = _in.length();
#endif

    __LDBG_printf("cmd=%u length=%u _in=%u discard=%u", _command, _length, _in.length(), (_command <= DISCARD || _in.length() == 0));
    if (_command <= DISCARD || _in.length() == 0) {
        _in.clear();
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
    Serial.printf_P(PSTR("SerialTwoWire %lu: transmissions=%u _in=%u "), (unsigned long)millis(), _transmissions, _inLen);
    _in.dumpAlloc(Serial);
    Serial.println();
#endif
}

void SerialTwoWireSlave::feed(uint8_t data)
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
            __LDBG_printf("discard data=%u", data);
            _discard();
        }
        else {
            __LDBG_assert_printf(_length < sizeof(_buffer) - 1, "buf=%-*.*s", (sizeof(_buffer) - 1), (sizeof(_buffer) - 1), _buffer);
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
            else if (_length >= sizeof(_buffer) - 1) { // buffer full
                __LDBG_printf("discard length=%u", _length);
                _discard();
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
    }
}

int SerialTwoWireSlave::_parseData(bool lastByte)
{
    if (_command <= DISCARD) {
        return kNoDataAvailable;
    }
#if I2C_OVER_UART_ADD_CRC16
    else if (_length > 4) {
        __LDBG_printf("discard length=%u", _length);
        _discard();
        return kNoDataAvailable;
    }
    else if (lastByte || _length == 4) {
        if (!_crcMarker || _crc == ~0 || (_length != 0 && _length != 4)) { // no marker, no data, invalid length
            __LDBG_printf("discard crc_marker=%u _crc=%04x", _crcMarker, _crc);
        }
        else {
            _buffer[4] = 0;
            auto crc = (uint16_t)strtoul(reinterpret_cast<const char *>(_buffer), nullptr, 16);
            if (crc == _crc) {
                return kNoDataAvailable;
            }
            __LDBG_printf("discard crc=%04x _crc=%04x", _crc, crc);
        }
        __LDBG_printf("discard length=%u", _length);
        _discard();
        return kNoDataAvailable;
    }
    else if (_crcMarker && _length < 4) { // && lastByte == false
        return kNoDataAvailable;
    }
#else
    else if (_length > 2) {
        __LDBG_assert_printf(_length <= 2, "cmd=%u len=%u last_byte=%u buf=%-*.*s", _command, _length, lastByte, (sizeof(_buffer) - 1), (sizeof(_buffer) - 1), _buffer);
        __LDBG_printf("discard length=%u", _length);
        _discard();
        return kNoDataAvailable;
    }
#endif
    else if (lastByte && _length == 1) {
        __LDBG_printf("discard length=%u", _length);
        _discard();
        return kNoDataAvailable;
    }
    else if (_length < 2) {
        return kNoDataAvailable;
    }
    __LDBG_assert_printf(_length <= 2, "cmd=%u len=%u last_byte=%u buf=%-*.*s", _command, _length, lastByte, (sizeof(_buffer) - 1), (sizeof(_buffer) - 1), _buffer);
    _buffer[2] = 0;
#if I2C_OVER_UART_ADD_CRC16
    auto data = (uint8_t)strtoul(reinterpret_cast<const char *>(_buffer), nullptr, 16);
    _crc = _crc16_update(_crc, data);
    return data;
#else
    return (uint8_t)strtoul(reinterpret_cast<const char *>(_buffer), nullptr, 16);
#endif
}

void SerialTwoWireSlave::_addBuffer(int data)
{
    if (data == kNoDataAvailable) {
        return;
    }
    if (_in.length()) {
        if (_in.length() >= kTransmissionMaxLength) {
            _discard();
        }
        else {
            // write to _in
            _in.write(data);
        }
    }
    else {
        __LDBG_assert_printf(_in.length() == 0, "length=%u data=%d", _length, data);
        if (data == _address) {
            // add address to buffer to indicate its use
            _in.write(data);
        }
        else {
            // invalid address, discard
            __LDBG_printf("address=%02x _address=%02x", data, _address);
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
            __LDBG_printf("available=%u addr=%02x _addr=%02x", _in.available(), address, _address);
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
