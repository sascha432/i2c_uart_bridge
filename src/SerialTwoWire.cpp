/**
 * Author: sascha_lammers@gmx.de
 */

#include "SerialTwoWire.h"

#if DEBUG_SERIALTWOWIRE
#include <PrintString.h>
#include <debug_helper.h>
#include <debug_helper_enable.h>
#else
#define __LDBG_printf(...)
#define __LDBG_print(...)
#define __LDBG_println(...)
#endif

#ifndef SERIALTWOWIRE_NO_GLOBALS

SerialTwoWire Wire;

void serialEvent() {
    Wire._serialEvent();
}

#endif

static const char _i2c_transmit_cmd[] PROGMEM = { I2C_OVER_UART_PREFIX_TRANSMIT };
static const char _i2c_request_cmd[] PROGMEM = { I2C_OVER_UART_PREFIX_REQUEST };

SerialTwoWire::SerialTwoWire() : SerialTwoWire(Serial)
{
}

SerialTwoWire::SerialTwoWire(Stream &serial) : _address(0), _recvAddress(0), _length(0), _command(NONE), _buffer{}, _serial(serial)
#ifndef SERIALTWOWIRE_NO_GLOBALS
    , _onReadSerial(serialEvent)
#endif
{
}

void SerialTwoWire::end()
{
    _length = 0;
    _command = NONE;
    _address = 0;
    _in.clear();
    _out.clear();
}

void SerialTwoWire::beginTransmission(uint8_t address)
{
    __LDBG_printf("address=0x%02x", address);
    _out.clear();
    _out.write(address);
}

uint8_t SerialTwoWire::endTransmission(uint8_t stop)
{
#if DEBUG_SERIALTWOWIRE
    PrintString str;
    str.print(reinterpret_cast<const __FlashStringHelper *>(_i2c_transmit_cmd));
    while (_out.available()) {
        str.printf_P(PSTR("%02x"), _out.read());
    }
    _out.clear();
    _serial.println(str);
    __DBG_printf("stop=%u out=%s", stop, __S(str));
#else
    _serial.print(reinterpret_cast<const __FlashStringHelper *>(_i2c_transmit_cmd));
    while (_out.available()) {
        _printHex(_out.read());
    }
    _out.clear();
    _serial.println();
#endif
    return 0;
}

uint8_t SerialTwoWire::requestFrom(uint8_t address, uint8_t count, uint8_t stop)
{
    __LDBG_printf("addr=0x%02x count=%u stop=%u", address, count, stop);
    _recvAddress = address;
    _serial.print(reinterpret_cast<const __FlashStringHelper *>(_i2c_request_cmd));
    _printHex(address);
    _printHex(count);
    _serial.println();
    return _waitForResponse();
}

uint8_t SerialTwoWire::_waitForResponse()
{
    if (_onReadSerial) {
        unsigned long timeout = millis() + _timeout;
        do {
#if defined(ESP8266) || defined(ESP32)
            delay(1);
#endif
            _onReadSerial();
        } while (millis() < timeout && !available());

        if (available()) {
            return _in.available();
        }
    }
    __LDBG_printf("timeout=%d can_yield=%u", (int)_timeout, can_yield());
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
    // return length after the entire line has been processed
    return _command == NONE && _in.available();
}

int SerialTwoWire::read(void)
{
    return _in.read();
}

int SerialTwoWire::peek(void)
{
    return _in.peek();
}

void SerialTwoWire::newLine()
{
    __LDBG_printf("cmd=%u buf=%s", _command, __S(printable_string(_buffer, _length)));
    if (_command > STOP_LINE) {
        _addBuffer();
        _processData();
    }
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
    else if (_length >= sizeof(_buffer) - 1) { // buffer full
        stopLine();
    }
    else if (_command == NONE) {
        if (_length || !isspace(data)) { // ignore leading spaces
            _buffer[_length++] = data;
            _buffer[_length] = 0;
            if (strcasecmp_P(reinterpret_cast<const char *>(_buffer), _i2c_transmit_cmd) == 0) {
                _command = TRANSMIT;
                _length = 0;
                _in.clear();
            }
            else if (strcasecmp_P(reinterpret_cast<const char *>(_buffer), _i2c_request_cmd) == 0) {
                _command = REQUEST;
                _length = 0;
                _in.clear();
            }
            __LDBG_printf("cmd=%u buf=%s len=%u", _command, _buffer, _length);
        }
    }
    else if (isxdigit(data)) {
        // add data to command buffer
        _buffer[_length++] = data;
        if (_length >= 2) {
            _addBuffer();
        }
    }
}


void SerialTwoWire::_addBuffer()
{
    if (_length >= 2) {
        _buffer[_length] = 0;
        auto data = strtoul(reinterpret_cast<const char *>(_buffer), nullptr, 16);
        if (_in.length() == 0 && data != _address && data != _recvAddress) { // stop processing if the address does not match and the data isn't a response to the last requestFrom() call
            __LDBG_printf("len=%u data=0x%02x address=0x%02x recv_addr=0x%02x", _in.length(), data, _address, _recvAddress);
            stopLine();
        }
        else {
            _in.write((uint8_t)data);
            _length = 0;
        }
    }
}

void SerialTwoWire::_processData()
{
    __LDBG_printf("len=%u addr=0x%02x _addr=0x%02x data=%s", _in.length(), _in.peek(), _address, __S(printable_string(_in.begin(), _in.length())));
    if (_in.length() > 1) {
        if (_command == TRANSMIT) {
            if (_in.read() == _address) {
                // address matches slave address, invoke onReceive callback
                _onReceive(_in.length());
            }
            else {
                // the data will be processed by after requestFrom()
                _recvAddress = 0;
            }
        }
        else if (_command == REQUEST) {
            // create requestFrom and send to serial
            uint8_t len = _in.charAt(1);
            beginTransmission((uint8_t)_address);
            _onRequest();
            while (_out.length() <= len) {
                _out.write(0xff);
            }
            endTransmission();
        }
    }
}

void SerialTwoWire::_serialEvent()
{
    while (_serial.available()) {
        feed(_serial.read());
    }
}
