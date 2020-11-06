/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "SerialTwoWireSlave.h"

#if DEBUG_SERIALTWOWIRE
#include <debug_helper.h>
#include <debug_helper_enable.h>
#endif

inline SerialTwoWireSlave::SerialTwoWireSlave() : SerialTwoWireSlave(Serial)
{
}

inline void SerialTwoWireSlave::begin(int address)
{
    begin((uint8_t)address);
}

inline void SerialTwoWireSlave::end()
{
    __LDBG_assert_printf(data()._address != kNotInitializedAddress, "end called without begin");
    _end();
}

inline void SerialTwoWireSlave::setClock(uint32_t)
{
}

inline bool SerialTwoWireSlave::isValidAddress(uint8_t address)
{
    return (address <= kMaxAddress) && (kMinAddress == 0 || (address >= kMinAddress));
}

inline size_t SerialTwoWireSlave::write(const char *data, size_t length)
{
    return write(reinterpret_cast<const uint8_t *>(data), length);
}

inline size_t SerialTwoWireSlave::available() const
{
    return _in.available();
}

inline size_t SerialTwoWireSlave::isAvailable()
{
    return _in.available();
}

inline int SerialTwoWireSlave::readByte()
{
    return _in.read();
}

inline int SerialTwoWireSlave::peekByte()
{
    return _in.peek();
}

inline size_t SerialTwoWireSlave::read(uint8_t *data, size_t length)
{
    return _in.readBytes(data, length);
}

inline size_t SerialTwoWireSlave::read(char *data, size_t length)
{
    return _in.readBytes(reinterpret_cast<uint8_t *>(data), length);
}

inline size_t SerialTwoWireSlave::write(int n)
{
    return write((uint8_t)n);
}

inline void SerialTwoWireSlave::setAllocMinSize(uint8_t size)
{
    _in.setAllocMinSize(size);
    _out.setAllocMinSize(size);
}

inline void SerialTwoWireSlave::releaseBuffers()
{
    _out.release();
    _in.release();
}

inline void SerialTwoWireSlave::setSerial(Stream &serial)
{
    _serial = &serial;
}

inline void SerialTwoWireSlave::onReceive(onReceiveCallback callback)
{
    _onReceive = callback;
}

inline void SerialTwoWireSlave::onRequest(onRequestCallback callback)
{
    _onRequest = callback;
}

inline void SerialTwoWireSlave::onReadSerial(onReadSerialCallback callback)
{
    _onReadSerial = callback;
}

inline size_t SerialTwoWireSlave::write(unsigned long n)
{
    return write((uint8_t)n);
}

inline size_t SerialTwoWireSlave::write(long n)
{
    return write((uint8_t)n);
}

inline size_t SerialTwoWireSlave::write(unsigned int n)
{
    return write((uint8_t)n);
}

inline void SerialTwoWireSlave::_discard()
{
    flags()._setCommand(CommandType::DISCARD);
}

inline size_t SerialTwoWireSlave::_printHex(uint8_t data)
{
    size_t written = _printNibble(data >> 4);
    return written + _printNibble(data & 0xf);
}

inline size_t SerialTwoWireSlave::_println()
{
    return _serial->write('\n');
}

inline size_t SerialTwoWireSlave::_printNibble(uint8_t nibble)
{
    return _serial->write(nibble < 0xa ? (nibble + '0') : (nibble + ('a' - 0xa)));
}

#if I2C_OVER_UART_ADD_CRC16

inline size_t SerialTwoWireSlave::_printHexCrc(uint16_t crc)
{
    size_t written = _serial->write(kCrcStartChar);
    // not big endian but for strtoul() ...
    written += _printHex(crc >> 8);
    written += _printHex((uint8_t)crc);
    return written + _println();
}

inline size_t SerialTwoWireSlave::_printHexUpdateCrc(uint8_t data, uint16_t &crc)
{
    crc = _crc16_update(crc, data);
    return _printHex(data);
}

#endif

inline SerialTwoWireSlave::Data_t &SerialTwoWireSlave::data()
{
    return _data;
}

inline SerialTwoWireSlave::Data_t &SerialTwoWireSlave::flags()
{
    return _data;
}

inline void SerialTwoWireSlave::_invokeOnReceive(int len)
{
    __LDBG_assert_printf(!!_onReceive, "_onReceive=%u callback=%p", !!_onReceive, &_onReceive);
    if (_onReceive) {
        flags()._readFromOut = false;
        _onReceive(len);
        flags()._readFromOut = true;
    }
}

inline void SerialTwoWireSlave::_invokeOnRequest()
{
    __LDBG_assert_printf(!!_onRequest, "_onRequest=%u callback=%p", !!_onRequest, &_onRequest);
    if (_onRequest) {
        flags()._readFromOut = false;
        _onRequest();
        flags()._readFromOut = true;
    }
}

inline void SerialTwoWireSlave::_invokeOnReadSerial()
{
    if (_onReadSerial) {
        _onReadSerial();
    }
}

inline Stream *SerialTwoWireSlave::getSerial() const {
    return _serial;
}

inline Stream &SerialTwoWireSlave::getSerial() {
    return *_serial;
}

inline const SerialTwoWireStream &SerialTwoWireSlave::readFrom() const
{
    return _in;
}

inline SerialTwoWireStream &SerialTwoWireSlave::readFrom()
{
    return _in;
}


inline void SerialTwoWireSlave::beginTransmission(int address)
{
    beginTransmission((uint8_t)address);
}



#if DEBUG_SERIALTWOWIRE
#include <debug_helper_disable.h>
#endif
