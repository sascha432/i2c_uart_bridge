/**
 * Author: sascha_lammers@gmx.de
 */

#include "SerialTwoWire.h"

#ifndef SERIALTWOWIRE_NO_GLOBALS

SerialTwoWire Wire;

void serialEvent() {
    Wire._serialEvent();
}

#endif

// prefix lengths are limited to 16 characters

#ifndef I2C_OVER_UART_PREFIX_TRANSMIT
#define I2C_OVER_UART_PREFIX_TRANSMIT           "+I2CT="
#endif
#ifndef I2C_OVER_UART_PREFIX_REQUEST
#define I2C_OVER_UART_PREFIX_REQUEST            "+I2CR="
#endif

static const char _i2c_transmit_cmd[] PROGMEM = { I2C_OVER_UART_PREFIX_TRANSMIT };
static const char _i2c_request_cmd[] PROGMEM = { I2C_OVER_UART_PREFIX_REQUEST };

SerialTwoWire::SerialTwoWire() : SerialTwoWire(Serial)
{
}

SerialTwoWire::SerialTwoWire(Stream &serial) : _address(0), _recvAddress(0), externalFeed(false), _command(NONE), _serial(serial)
{
#ifndef SERIALTWOWIRE_NO_GLOBALS
    _onReadSerial = serialEvent;
#endif
}

void SerialTwoWire::setSerial(Stream &serial)
{
    _serial = serial;
}

void SerialTwoWire::beginTransmission(uint8_t address)
{
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
    if (_command > NONE) {
        _addBuffer();
        _processData();
    }
    _command = NONE;
    _buffer = String();
}


void SerialTwoWire::feed(uint8_t data)
{
    if (data == '\r') {
    }
    else if (data == '\n') {
        // #4 line done
        newLine();
    }
    else if (_command == STOP_LINE) {
        // #3 skip rest of the line cause of invalid data
    }
    else if (_command != NONE) {
        // #2 add data to command buffer
        if (data != ',') {
            _buffer += (char)data;
            if (_buffer.length() == 2) {
                _addBuffer();
            }
        }
    }
    else if ((_buffer.length() > 0 || !isspace(data)) && _buffer.length() < 16) {
        // #1 trim leading spaces and determine command
        _buffer += (char)data;
        if (strcasecmp_P(_buffer.c_str(), _i2c_transmit_cmd) == 0) {
            _in.clear();
            _command = TRANSMIT;
            _buffer = String();
        }
        else if (strcasecmp_P(_buffer.c_str(), _i2c_request_cmd) == 0) {
            _in.clear();
            _command = REQUEST;
            _buffer = String();
        }
    }
}

void SerialTwoWire::stopLine()
{
    _command = STOP_LINE;
}

void SerialTwoWire::onReceive(onReceiveCallback callback)
{
    _onReceive = callback;
}

void SerialTwoWire::onRequest(onRequestCallback callback)
{
    _onRequest = callback;
}

void SerialTwoWire::onReadSerial(onReadSerialCallback callback)
{
    _onReadSerial = callback;
}

void SerialTwoWire::_addBuffer()
{
    if (_buffer.length() == 2) {
        auto data = strtoul(_buffer.c_str(), 0, 16);
        if (_in.length() == 0 && data != _address && data != _recvAddress) { // stop processing if the address does not match and the data isn't a response to the last requestFrom() call
            stopLine();
        }
        else {
            _in.write((uint8_t)data);
            _buffer = String();
        }
    }
}

void SerialTwoWire::_processData()
{
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
            beginTransmission(_address);
            _onRequest();
            while (_out.length() <= len) {
                _out.write(0xff);
            }
            endTransmission();
        }
    }
}

void SerialTwoWire::_printHex(uint8_t data)
{
    _serial.print(data >> 4, HEX);
    _serial.print(data & 0xf, HEX);
}

void SerialTwoWire::_serialEvent()
{
    while (_serial.available()) {
        feed(_serial.read());
    }
}
