/**
 * Author: sascha_lammers@gmx.de
 */

#include "SerialTwoWire.h"
#include "SerialTwoWireMaster.h"
#include "SerialTwoWireDebug.h"

#pragma push_macro("new")
#undef new

uint8_t SerialTwoWireMaster::requestFrom(uint8_t address, uint8_t count, uint8_t stop)
{
    __LDBG_printf("addr=%02x count=%u stop=%u len=%u outs=%u", address, count, stop, _request().length(), flags()._outState);

    if (count == 0 || !isValidAddress(address)) {
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
    // write as fast as possible
    _serial->flush();
    size_t written = _serial->print(FPSTR(requestCommand));
    written += _printHex(address);
    written += _printHex(count);
#if I2C_OVER_UART_ADD_CRC16
    written += _printHexCrc(crc);
    __LDBG_assert_printf(written == kRequestCommandLength + 10, "written=%u expected=%u", written,kRequestCommandLength + 10);
    if (written != kRequestCommandLength + 10) {
        return 0;
    }
#else
    written += _println();
    __LDBG_assert_printf(written == kRequestCommandLength + 5, "written=%u expected=%u", written,kRequestCommandLength + 5);
    if (written != kRequestCommandLength + 5) {
        return 0;
    }
#endif
    _serial->flush();

    // wait for response
#if DEBUG_SERIALTWOWIRE
    auto start = micros();
    auto result = _waitForResponse(address, count);
    auto dur = micros() - start;
    __DBG_printf("response=%d time=%uus can_yield=%u outs=%u", result, dur, can_yield(), flags()._outState);
    return result;
#else
    return _waitForResponse(address, count);
#endif
}

uint8_t SerialTwoWireMaster::_waitForResponse(uint8_t address, uint8_t count)
{
    unsigned long timeout = millis() + _timeout;
    while(flags()._outIsFilling() && millis() <= timeout) {
        optimistic_yield(1000);
        _invokeOnReadSerial();
    }
    __LDBG_printf("count=%u _ravail=%u _rlen=%u outs=%u", _request().charAt(0), _request().available(), _request().length(), flags()._outState);
    if (flags()._getOutState() == OutStateType::FILLED && !_request().empty() && _request().read() == address) {
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


void SerialTwoWireMaster::beginTransmission(uint8_t address)
{
    __LDBG_printf("addr=%02x outs=%u", address, flags()._outState);
    flags()._setOutState(OutStateType::LOCKED);
    _out.clear();
    _out.write(address);
}

uint8_t SerialTwoWireMaster::endTransmission(uint8_t stop)
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
        return _endTransmission(stop);
    }
    __LDBG_printf("code=%d oavail=%u olen=%u slave=%d master=%d outs=%u ins=%u", code, _out.available(), _out.length(), address, data()._address, flags()._outState, flags()._inState);
    _out.clear();
    flags()._setOutState(OutStateType::NONE);
    return static_cast<uint8_t>(code);

}

uint8_t SerialTwoWireMaster::_endTransmission(uint8_t stop)
{
    auto iter = _out.begin();
    auto end = _out.end();
#if I2C_OVER_UART_ADD_CRC16
    uint16_t crc = crc16_update(iter, end - iter);
#endif
    // write as fast as possible
    _serial->flush();
    _serial->print(FPSTR(transmitCommand));
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

int SerialTwoWireMaster::available()
{
    return isAvailable();
}

int SerialTwoWireMaster::read()
{
    return readByte();
}

int SerialTwoWireMaster::peek()
{
    return peekByte();
}

void SerialTwoWireMaster::_newLine()
{
    // add any data that's left in the buffer
    _addBuffer(_parseData(true));

    __LDBG_printf("cmd=%u len=%u ilen=%u rlen=%u discard=%u outs=%u ins=%u",
        flags()._command, data()._length, _in.length(),
        (flags()._getOutState() == OutStateType::FILLING ? _request().length() : 0),
        (flags()._command <= DISCARD || (_in.length() == 0 && flags()._outIsFilling() == false)),
        flags()._outState,
        flags()._inState
    );

    if (flags()._command > DISCARD && (flags()._inState || flags()._outIsFilling())) {
        _processData();
    }
    _cleanup();
}

void SerialTwoWireMaster::_addBuffer(int byte)
{
    if (byte == kNoDataAvailable) {
        return;
    }
    //__LDBG_printf("data=%02x _addr=%02x _request=%02x outs=%u _ravail=%u _rlen=%u", byte, data()._address, _request().charAt(0) & 0xffff, flags()._outState, _request().available(), _request().length());
    if (flags()._getOutState() == OutStateType::FILLING) {
        if (_request().length() >= kTransmissionMaxLength) {
            __LDBG_printf("data=%d rlen=%u max=%u", byte, _request().length(), kTransmissionMaxLength);
            _discard();
        }
        else {
            // __LDBG_printf("data=%u outs=%u avail=%u len=%u", byte, flags()._outState, _request().available(), _request().length());
            _request().write(byte);
        }
    }
    else if (flags()._inState) {
        // write to _in
        if (flags()._command == REQUEST && _in.length() >= kRequestTransmissionMaxLength) {
            __LDBG_printf("data=%d ilen=%u max=%u", byte, _in.length(), kRequestTransmissionMaxLength);
            _discard();
        }
        else if (_in.length() >= kTransmissionMaxLength) {
            __LDBG_printf("data=%d ilen=%u max=%u", byte, _in.length(), kTransmissionMaxLength);
            _discard();
        }
        else {
            _in.write(byte);
        }
    }
    else {
        __LDBG_assert_printf(_in.length() == 0, "len=%u data=%d", data()._length, byte);
        if (byte == data()._address) {
            // mark as being in use
            flags()._inState = true;
        }
        else if (_request().length() == 1 && flags()._getOutState() == OutStateType::FILL && _request()[0] == byte) {
            // mark as being processed
            flags()._setOutState(OutStateType::FILLING);
            // the address was added by requestFrom() already
            // keep it int the buffer for waitForResponse
            __LDBG_printf("addr=%02x outs=%u ravail=%u rlen=%u", _request()[0], flags()._outState, _request().available(), _request().length());
        }
        else {
            // invalid address, discard
            __LDBG_printf("addr=%02x _addr=%02x _request=%02x outs=%u", byte, data()._address, _request().charAt(0) & 0xffff, flags()._outState);
            _discard();
        }
    }
    data()._length = 0;
}

void SerialTwoWireMaster::_processData()
{
    _preProcess();

    switch (flags()._command) {
    case CommandEnum_t::REQUEST:
        if (true) {
            // request has address and length only
            __LDBG_printf("requestFrom addr=%02x len=%u", data()._address, in.charAt(0));
            _in.clear();
            if (flags()._getOutState() != OutStateType::NONE) {
                // cannot accept request while requestFrom() is waiting
                _sendNack(data()._getAddress());
            }
            else {
                beginTransmission(data()._getAddress());
                // collect data in output buffer
                _invokeOnRequest();
                _endTransmission(true);
            }
        }
        break;
    case CommandEnum_t::TRANSMIT:
        if (flags()._inState) {
            __LDBG_assert_printf(_in.length() == _in.available(), "ilen=%u iavail=%u", _in.length(), _in.available());
            __LDBG_printf("iavail=%u ilen=%u _addr=%02x", _in.available(), _in.length(), data()._address);
            _invokeOnReceive(_in.available());
        }
        else if (flags()._getOutState() == OutStateType::FILLING) {
            __LDBG_assert_printf(_request().length() == _request().available(), "rlen=%u ravail=%u", _request().length(), _request().available());
            // mark as finished
            flags()._setOutState(OutStateType::FILLED);
            __LDBG_printf("addr=%02x ravail=%u outs=%u", _request().peek(), _request().available(), flags()._outState);
        }
        break;
    }
}

void SerialTwoWireMaster::feed(uint8_t byte)
{
    if (byte == '\n') { // check first
        _newLine();
    }
    else if (flags()._command == CommandEnum_t::DISCARD || flags()._command == CommandEnum_t::SEND_DISCARDED || byte == '\r') {
        // skip rest of the line cause of invalid data
    }
    else if (flags()._command == NONE) {
        static_assert(kCommandMaxLength < sizeof(_buffer), "invalid size");
        if ((data()._length == 0 && byte != '+') || data()._length >= kCommandMaxLength) {
            _sendAndDiscard();
        }
        else {
            // append
            _buffer[data()._length++] = byte;
            _buffer[data()._length] = 0;
            if (strcasecmp_P(reinterpret_cast<const char *>(_buffer), transmitCommand) == 0) {
                flags()._command = TRANSMIT;
                data()._length = 0;
                _newTransmission();
            }
            else if (strcasecmp_P(reinterpret_cast<const char *>(_buffer), requestCommand) == 0) {
                flags()._command = REQUEST;
                data()._length = 0;
                _newTransmission();
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
