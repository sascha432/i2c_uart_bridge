/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "SerialTwoWireMaster.h"

#if DEBUG_SERIALTWOWIRE
#include <debug_helper.h>
#include <debug_helper_enable.h>
#endif

inline void SerialTwoWireMaster::begin()
{
    __LDBG_assert_printf(data()._address == kNotInitializedAddress, "begin called again without end");
    data()._address = kMasterAddress;
}

inline uint8_t SerialTwoWireMaster::requestFrom(int address, int count, int stop)
{
    return requestFrom((uint8_t)address, (uint8_t)count, (uint8_t)stop);
}

inline size_t SerialTwoWireMaster::available() const
{
    return readFrom().available();
}

inline size_t SerialTwoWireMaster::isAvailable()
{
    return readFrom().available();
}

inline int SerialTwoWireMaster::readByte()
{
    return readFrom().read();
}

inline int SerialTwoWireMaster::peekByte()
{
    return readFrom().peek();
}

inline stream_read_return_t SerialTwoWireMaster::read(uint8_t *data, size_t length)
{
    return readFrom().readBytes(reinterpret_cast<uint8_t *>(data), length);
}

inline size_t SerialTwoWireMaster::read(char *data, size_t length)
{
    return read(reinterpret_cast<uint8_t *>(data), length);
}

inline const SerialTwoWireStream &SerialTwoWireMaster::readFrom() const
{
    return _data._readFromOut ? _out : _in;
}

inline SerialTwoWireStream &SerialTwoWireMaster::readFrom()
{
    return _data._readFromOut ? _out : _in;
}

inline SerialTwoWireStream &SerialTwoWireMaster::_request()
{
    return _out;
}

#if DEBUG_SERIALTWOWIRE
#include <debug_helper_disable.h>
#endif
