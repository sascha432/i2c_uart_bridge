/**
 * Author: sascha_lammers@gmx.de
 */

#include "SerialTwoWire.h"
#include "SerialTwoWireSlave.h"
#include "SerialTwoWireDebug.h"

SerialTwoWireSlave::SerialTwoWireSlave() :
    SerialTwoWireSlave(Serial)
{
}

SerialTwoWireSlave::SerialTwoWireSlave(Stream &serial, onReadSerialCallback callback) :
    _data(),
    _buffer{},
    _onReceive(nullptr),
    _onRequest(nullptr),
    _onReadSerial(callback),
    _serial(&serial)
{
}

void SerialTwoWireSlave::begin(uint8_t address)
{
    end();
    data()._address = address;
    _in.reserve(SerialTwoWireStream::kAllocMinSize);
    _out.reserve(SerialTwoWireStream::kAllocMinSize);
}

void SerialTwoWireSlave::end()
{
    data() = Data_t();
    _in.release();
    _out.release();
}

void SerialTwoWireSlave::beginTransmission(uint8_t address)
{
    __LDBG_printf("addr=%02x out_state=%u", address, flags()._outState);
    flags()._setOutState(OutStateType::LOCKED);
    _out.clear();
    _out.write(address);
}

uint8_t SerialTwoWireSlave::endTransmission(uint8_t stop)
{
    __LDBG_assert_printf(flags()._getOutState() == OutStateType::LOCKED, "out_state=%u", flags()._outState);
    if (flags()._getOutState() != OutStateType::LOCKED) {
        __LDBG_printf("out_state=%u", flags()._outState);
        return 0;
    }
    __LDBG_assert_printf(_out.length() && _out.charAt(0) != data()._address, "len=%u slave=%u master=%u", _out.length(), _out.charAt(0), data()._address);
    if (_out.charAt(0) == data()._address) {
        __LDBG_printf("len=%u slave=%u master=%u out_state=%u", _out.length(), _out.charAt(0), data()._address, flags()._outState);
        _out.clear();
        flags()._setOutState(OutStateType::NONE);
        return 0;
    }
    return _endTransmission(stop);
}

uint8_t SerialTwoWireSlave::_endTransmission(uint8_t stop)
{
    auto iter = _out.begin();
    auto end = _out.end();
#if I2C_OVER_UART_ADD_CRC16
    uint16_t crc = crc16_update(iter, end - iter);
#endif
    // write as fast as possible
    auto ptr = transmitCommand;
    uint8_t data;
    _serial->flush();
    while((data = pgm_read_byte(ptr++)) != 0) {
        _serial->write(data);
    }
    for(; iter != end; ++iter) {
        _printHex(*iter);
    }
#if I2C_OVER_UART_ADD_CRC16
    _printHexCrc(crc);
#else
    _println();
#endif
    _serial->flush();
    _out.clear();
    flags()._setOutState(OutStateType::NONE);

    return 0;
}

size_t SerialTwoWireSlave::write(uint8_t data)
{
    __LDBG_assert_printf(flags()._outCanWrite(), "out_state=%u", flags()._outState);
    if (!flags()._outCanWrite()) {
        __LDBG_printf("out_state=%u", flags()._outState);
        return 0;
    }
    return _out.write(data);
}

size_t SerialTwoWireSlave::write(const uint8_t *data, size_t length)
{
    __LDBG_assert_printf(flags()._outCanWrite(), "out_state=%u", flags()._outState);
    if (!flags()._outCanWrite()) {
        __LDBG_printf("out_state=%u", flags()._outState);
        return 0;
    }
    return _out.write(data, (SerialTwoWireStream::size_type)length);
}

int SerialTwoWireSlave::available()
{
    return _in.available();
}

int SerialTwoWireSlave::read()
{
    auto data = _in.read();
    if (_in.empty()) {
        _in.clear();
    }
    return data;
}

int SerialTwoWireSlave::peek()
{
    return _in.peek();
}

#if defined(ESP8266)

size_t SerialTwoWireSlave::readBytes(uint8_t *buffer, size_t length)
{
    return _in.readBytes(buffer, length);
}

size_t SerialTwoWireSlave::readBytes(char *buffer, size_t length)
{
    return _in.readBytes(reinterpret_cast<uint8_t *>(buffer), length);
}

#endif

void SerialTwoWireSlave::_newLine()
{
    // add any data left in the buffer
    _addBuffer(_parseData(true));

    __LDBG_printf("cmd=%u length=%u _in=%u discard=%u out_state=%u", flags()._command, data()._length, _in.length(), (flags()._command <= DISCARD || _in.length() == 0), flags()._outState);
    if (flags()._command <= DISCARD || _in.length() == 0) {
        _in.clear();
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

void SerialTwoWireSlave::feed(uint8_t byte)
{
    if (byte == '\n') { // check first
        _newLine();
    }
    else if (flags()._command == DISCARD || byte == '\r') {
        // skip rest of the line cause of invalid data
    }
    else if (flags()._command == NONE) {
        //static_assert(kMaxTransmissionLength < sizeof(_buffer), "invalid size");
        if ((data()._length == 0 && byte != '+') || data()._length >= kMaxTransmissionLength) {
            // must start with +
            __LDBG_printf("discard data=%u", byte);
            _discard();
        }
        else {
            __LDBG_assert_printf(data()._length < sizeof(_buffer) - 1, "buf=%-*.*s", (sizeof(_buffer) - 1), (sizeof(_buffer) - 1), _buffer);
            // append
            _buffer[data()._length++] = byte;
            _buffer[data()._length] = 0;
            if (strcasecmp_P(reinterpret_cast<const char *>(_buffer), transmitCommand) == 0) {
                flags()._command = TRANSMIT;
                data()._length = 0;
            }
            else if (data()._length >= sizeof(_buffer) - 1) { // buffer full
                __LDBG_printf("discard length=%u", data()._length);
                _discard();
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
    }
}

int SerialTwoWireSlave::_parseData(bool lastByte)
{
    if (flags()._command <= DISCARD) {
        return kNoDataAvailable;
    }
#if I2C_OVER_UART_ADD_CRC16
    else if (data()._length > 4) {
        __LDBG_printf("discard length=%u", data()._length);
        _discard();
        return kNoDataAvailable;
    }
    else if (lastByte || data()._length == 4) {
        if (!flags()._crcMarker || flags()._crc == ~0 || (data()._length != 0 && data()._length != 4)) { // no marker, no data, invalid length
            __LDBG_printf("discard crc_marker=%u _crc=%04x", flags()._crcMarker, flags()._crc);
        }
        else {
            _buffer[4] = 0;
            auto crc = (uint16_t)strtoul(reinterpret_cast<const char *>(_buffer), nullptr, 16);
            if (crc == flags()._crc) {
                return kNoDataAvailable;
            }
            __LDBG_printf("discard crc=%04x flags()._crc=%04x", flags()._crc, crc);
        }
        __LDBG_printf("discard length=%u", data()._length);
        _discard();
        return kNoDataAvailable;
    }
    else if (flags()._crcMarker && data()._length < 4) { // && lastByte == false
        return kNoDataAvailable;
    }
#else
    else if (data()._length > 2) {
        __LDBG_assert_printf(data()._length <= 2, "cmd=%u len=%u last_byte=%u buf=%-*.*s", flags()._command, data()._length, lastByte, (sizeof(_buffer) - 1), (sizeof(_buffer) - 1), _buffer);
        __LDBG_printf("discard length=%u", data()._length);
        _discard();
        return kNoDataAvailable;
    }
#endif
    else if (lastByte && data()._length == 1) {
        __LDBG_printf("discard length=%u", data()._length);
        _discard();
        return kNoDataAvailable;
    }
    else if (data()._length < 2) {
        return kNoDataAvailable;
    }
    __LDBG_assert_printf(data()._length <= 2, "cmd=%u len=%u last_byte=%u buf=%-*.*s", flags()._command, data()._length, lastByte, (sizeof(_buffer) - 1), (sizeof(_buffer) - 1), _buffer);
    _buffer[2] = 0;
#if I2C_OVER_UART_ADD_CRC16
    auto data = (uint8_t)strtoul(reinterpret_cast<const char *>(_buffer), nullptr, 16);
    flags()._crc = _crc16_update(flags()._crc, data);
    return data;
#else
    return (uint8_t)strtoul(reinterpret_cast<const char *>(_buffer), nullptr, 16);
#endif
}

void SerialTwoWireSlave::_addBuffer(int byte)
{
    if (byte == kNoDataAvailable) {
        return;
    }
    if (_in.length()) {
        if (_in.length() >= kTransmissionMaxLength) {
            _discard();
        }
        else {
            // write to _in
            _in.write(byte);
        }
    }
    else {
        __LDBG_assert_printf(_in.length() == 0, "length=%u data=%d", data()._length, byte);
        if (byte == data()._address) {
            // add address to buffer to indicate its use
            _in.write(byte);
        }
        else {
            // invalid address, discard
            __LDBG_printf("addr=%02x _addr=%02x", byte, data()._address);
            _discard();
        }
    }
    data()._length = 0;
}

void SerialTwoWireSlave::_processData()
{
    if (_in.length() == 1) {
        // no data, discard
        _in.clear();
    }
    if (flags()._command == TRANSMIT) {
        if (_in.length()) {
            __LDBG_printf("available=%u addr=%02x _addr=%02x", _in.available(), _in.peek(), data()._address);
            _in.read();
            // address was already checked
            _onReceive(_in.available());
            _in.clear();
        }
    }
}

void SerialTwoWireSlave::_serialEvent()
{
    while (_serial->available()) {
        feed(_serial->read());
    }
}
