/**
 * Author: sascha_lammers@gmx.de
 */

#include "SerialTwoWire.h"
#include "SerialTwoWireSlave.h"
#include "SerialTwoWireDebug.h"

#if DEBUG_SERIALTWOWIRE
#include <debug_helper.h>
#include <debug_helper_enable.h>
#endif

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
    __LDBG_assert_printf(isValidAddress(address), "address=%u min=%u max=%u", address, kMinAddress, kMaxAddress);
    __LDBG_assert_printf(data()._address == kNotInitializedAddress, "begin called again without end");
    _end();
    data()._address = address;
    _in.reserve(SerialTwoWireStream::kAllocMinSize);
    _out.reserve(SerialTwoWireStream::kAllocMinSize);
#if DEBUG_SERIALTWOWIRE
    _startMillis = millis();
#endif
}

void SerialTwoWireSlave::_end()
{
    if (data()._address != kNotInitializedAddress) {
        data() = Data_t();
        _in.release();
        _out.release();
    }
}

size_t SerialTwoWireSlave::write(uint8_t data)
{
    __LDBG_assert_printf(flags()._outCanWrite(), "outs=%u", flags()._outState);
    if (!flags()._outCanWrite()) {
        __LDBG_printf("outs=%u", flags()._outState);
        return 0;
    }
    return _out.write(data);
}

size_t SerialTwoWireSlave::write(const uint8_t *data, size_t length)
{
    __LDBG_assert_printf(flags()._outCanWrite(), "outs=%u", flags()._outState);
    if (!flags()._outCanWrite()) {
        __LDBG_printf("outs=%u", flags()._outState);
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
    return _in.read();
}

int SerialTwoWireSlave::peek()
{
    return _in.peek();
}

void SerialTwoWireSlave::_newLine()
{
    // add any data left in the buffer
    _addBuffer(_parseData(true));

    __LDBG_printf("cmd=%u len=%u ilen=%u discard=%u outs=%u ins=%u", flags()._command, data()._length, _in.length(), (flags()._command <= DISCARD || _in.length() == 0), flags()._outState, flags()._inState);
    if (flags()._getCommand() > CommandType::DISCARD && flags()._inState) {
        _processData();
    }
    _cleanup();
}

void SerialTwoWireSlave::_sendAndDiscard()
{
#if I2C_OVER_UART_ENABLE_DISCARD_ID

    if (data()._length != 0 && *_buffer == '+') {           // collect discarded message
        _in.clear();
        #if I2C_OVER_UART_ENABLE_DISCARD_ID != 43
            _in.write(I2C_OVER_UART_ENABLE_DISCARD_ID);     // add id to transmission if not '+'
        #endif
        _in.write(_buffer, sizeof(_buffer));
        __LDBG_printf("send_and_discard id=%d ilen=%u", _in.charAt(0), _in.length());
        flags()._setCommand(CommandType::SEND_DISCARDED);
        flags()._inState = true;
        data()._length = 0;
        _newTransmission();
        return;
    }

#endif
    __LDBG_printf("discard len=%u max=%u", data()._length, kCommandMaxLength);
    data()._length = 0;
    _discard();
}

void SerialTwoWireSlave::_cleanup()
{
    flags()._setCommand(CommandType::NONE);
    data()._length = 0;
    if (flags()._inState) {
        _in.clear();
        flags()._inState = false;
    }
    //if (flags()._outIsFilling()) {
    //    flags()._setOutState(OutStateType::NONE);
    //}
#if I2C_OVER_UART_ADD_CRC16
    flags()._crcMarker = false;
    flags()._crc = ~0;
#endif
}

void SerialTwoWireSlave::_sendNack(uint8_t address)
{
    _serial->flush();
    sendCommandStr(*_serial, CommandStringType::SLAVE_RESPONSE);
#if I2C_OVER_UART_ADD_CRC16
    // TODO
    _printHex(address);
    _println();
#else
    _printHex(address);
    _println();
#endif
    _serial->flush();
}

void SerialTwoWireSlave::feed(uint8_t byte)
{
    if (byte == '\n') { // check first
        _newLine();
    }
    else if (flags()._getCommand() == CommandType::DISCARD || flags()._getCommand() == CommandType::SEND_DISCARDED || byte == '\r') {
        // skip rest of the line cause of invalid data
    }
    else if (flags()._getCommand() == CommandType::NONE) {
        //static_assert(kCommandMaxLength < sizeof(_buffer), "invalid size");
        if ((data()._length == 0 && byte != '+') || data()._length >= kCommandMaxLength) {
            _sendAndDiscard();
        }
        else {
            __LDBG_assert_printf(data()._length < sizeof(_buffer) - 1, "buf=%-*.*s", (sizeof(_buffer) - 1), (sizeof(_buffer) - 1), _buffer);
            // append
            _buffer[data()._length++] = byte;
            _buffer[data()._length] = 0;
            switch(getCommandStringType(_buffer)) {
                case CommandStringType::MASTER_TANSMIT:
                    flags()._setCommand(CommandType::MASTER_TRANSMIT);
                    data()._length = 0;
                    _newTransmission();
                    break;
                case CommandStringType::MASTER_REQUEST:
                    flags()._setCommand(CommandType::MASTER_REQUEST);
                    data()._length = 0;
                    _newTransmission();
                    break;
                default:
                    break;
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
    if (flags()._getCommand() <= CommandType::DISCARD) {
        return kNoDataAvailable;
    }
#if I2C_OVER_UART_ADD_CRC16
    else if (data()._length > 4) {
        __LDBG_printf("discard len=%u", data()._length);
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
        __LDBG_printf("discard len=%u", data()._length);
        _discard();
        return kNoDataAvailable;
    }
    else if (flags()._crcMarker && data()._length < 4) { // && lastByte == false
        return kNoDataAvailable;
    }
#else
    else if (data()._length > 2) {
        __LDBG_assert_printf(data()._length <= 2, "cmd=%u len=%u last_byte=%u buf=%-*.*s", flags()._command, data()._length, lastByte, (sizeof(_buffer) - 1), (sizeof(_buffer) - 1), _buffer);
        __LDBG_printf("discard len=%u", data()._length);
        _discard();
        return kNoDataAvailable;
    }
#endif
    else if (lastByte && data()._length == 1) {
        __LDBG_printf("discard len=%u", data()._length);
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
    if (flags()._inState) {
        if (_in.length() >= kTransmissionMaxLength) {
            __LDBG_printf("data=%u ilen=%u max=%u", byte, _in.length(), kTransmissionMaxLength);
            _discard();
        }
        else {
            // write to _in
            _in.write(byte);
        }
    }
    else {
        __LDBG_assert_printf(_in.length() == 0, "len=%u data=%d", data()._length, byte);
        if (byte == data()._address) {
            // mark as being in use
            flags()._inState = true;
        }
        else {
            // invalid address, discard
            __LDBG_printf("addr=%02x _addr=%02x", byte, data()._address);
            _discard();
        }
    }
    data()._length = 0;
}

void SerialTwoWireSlave::_preProcess()
{
    if (flags()._inState && _in.length() == 0) {
        // no data, discard
        __LDBG_printf("iavail=%u ilen=%u", _in.available(), _in.length());
        _discard();
    }
#if I2C_OVER_UART_ENABLE_DISCARD_COMMAND
    switch (flags()._getCommand()) {
    case CommandType::SEND_DISCARDED:
        __LDBG_printf("iavail=%u ilen=%u id=%02x", _in.available(), _in.length(), _in.charAt(0));
        flags()._readFromOut = false;
        _onReceive(_in.available());
        flags()._readFromOut = true;
        break;
    }
#endif
}

void SerialTwoWireSlave::_processData()
{
    _preProcess();

    switch(flags()._getCommand()) {
    case CommandType::MASTER_REQUEST:
        if (true) {
            // request has address and length only
            __LDBG_printf("requestFrom addr=%02x len=%u", data()._address, _in.charAt(0));
            _in.clear();
            if (flags()._getOutState() != OutStateType::NONE) {
                // cannot accept request while requestFrom() is waiting
                _sendNack(data()._getAddress());
                return;
            }
            beginTransmission(data()._getAddress());
            // collect data in output buffer
            _invokeOnRequest();
            _endTransmission(CommandStringType::SLAVE_RESPONSE, true);
        }
        break;
    case CommandType::MASTER_TRANSMIT:
        if (flags()._inState) {
            __LDBG_assert_printf(_in.length() == _in.available(), "ilen=%u iavail=%u", _in.length(), _in.available());
            __LDBG_printf("iavail=%u ilen=%u _addr=%02x", _in.available(), _in.length(), data()._address);
            flags()._readFromOut = false;
            _onReceive(_in.available());
            flags()._readFromOut = true;
        }
        break;
    default:
        break;
    }
}

const char *SerialTwoWireSlave::getCommandStr(CommandStringType type)
{
    static char buf[8];
    switch(type) {
        case CommandStringType::MASTER_REQUEST:
        case CommandStringType::SLAVE_RESPONSE:
        case CommandStringType::MASTER_TANSMIT:
            snprintf_P(buf, sizeof(buf), PSTR("+I2C%c="), type);
            return buf;
        default:
            break;
    }
    return nullptr;
}

size_t SerialTwoWireSlave::sendCommandStr(Stream &stream, CommandStringType type)
{
    if (type == CommandStringType::NONE) {
        return 0;
    }
    return stream.print(getCommandStr(type));
}

SerialTwoWireSlave::CommandStringType SerialTwoWireSlave::getCommandStringType(const char *str)
{
    if (*str == '+') {
        if (strcasecmp(str, getCommandStr(CommandStringType::MASTER_REQUEST)) == 0) {
            return CommandStringType::MASTER_REQUEST;
        }
        if (strcasecmp(str, getCommandStr(CommandStringType::SLAVE_RESPONSE)) == 0) {
            return CommandStringType::SLAVE_RESPONSE;
        }
        if (strcasecmp(str, getCommandStr(CommandStringType::MASTER_TANSMIT)) == 0) {
            return CommandStringType::MASTER_TANSMIT;
        }
    }
    return CommandStringType::NONE;
}

void SerialTwoWireSlave::beginTransmission(uint8_t address)
{
    __LDBG_printf("addr=%02x outs=%u", address, flags()._outState);
    flags()._setOutState(OutStateType::LOCKED);
    _out.clear();
    _out.write(address);
}

uint8_t SerialTwoWireSlave::endTransmission(uint8_t stop)
{
    EndTransmissionCode code = EndTransmissionCode::SUCCESS;
    int address = _out.peek();
    __LDBG_assert_printf(flags()._getOutState() == OutStateType::LOCKED && _out.available() && address != data()._address && isValidAddress(address), "oavail=%u olen=%u slave=%d master=%d outs=%u ins=%u", _out.available(), _out.length(), address, data()._address, flags()._outState, flags()._inState);
    if (flags()._getOutState() != OutStateType::LOCKED) {
        code = EndTransmissionCode::END_WITHOUT_BEGIN;
    }
    else if (!isValidAddress(address)) {
        __LDBG_printf("oavail=%u olen=%u slave=%d master=%d outs=%u ins=%u", _out.available(), _out.length(), address, data()._address, flags()._outState, flags()._inState);
        code = EndTransmissionCode::INVALID_ADDRESS;
    }
    else if (address == data()._address) {
        code = EndTransmissionCode::OWN_ADDRESS;
    }
    if (code == EndTransmissionCode::SUCCESS) {
        return _endTransmission(CommandStringType::MASTER_TANSMIT, stop);
    }
    __LDBG_printf("code=%d oavail=%u olen=%u slave=%d master=%d outs=%u ins=%u", code, _out.available(), _out.length(), address, data()._address, flags()._outState, flags()._inState);
    _out.clear();
    flags()._setOutState(OutStateType::NONE);
    return static_cast<uint8_t>(code);

}

uint8_t SerialTwoWireSlave::_endTransmission(CommandStringType type, uint8_t stop)
{
    auto iter = _out.begin();
    auto end = _out.end();
#if I2C_OVER_UART_ADD_CRC16
    uint16_t crc = crc16_update(iter, end - iter);
#endif
    // write as fast as possible
    _serial->flush();
    sendCommandStr(*_serial, type);
    for (; iter != end; ++iter) {
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

    return static_cast<uint8_t>(EndTransmissionCode::SUCCESS);
}
