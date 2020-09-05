/**
 * Author: sascha_lammers@gmx.de
 */

//
// Extension to match functionality and method names
//

#pragma once

#include "SerialTwoWireDef.h"

class SerialTwoWireSlave;
class SerialTwoWireMaster;

using namespace SerialTwoWireDef;

#if SERIALTWOWIRE_USE_OWN_STREAM_CLASS

//
// small, fast, better dynamic memory mamangement and does not required much memory
//
// numbers for 32bit alignment
// static constexpr size_t SerialTwoWireStreamSize = sizeof(SerialTwoWireStream);          // 12
// static constexpr size_t BufferStreamSize = sizeof(BufferStream);                        // 44
// static constexpr size_t StreamStringSize = sizeof(StreamString);                        // 52

constexpr bool __constexpr_is_bitmask(size_t num, uint16_t bits = 15) {
    return num == 0U || bits == 0U ? false : ((num == (1U << bits) ? true : __constexpr_is_bitmask(num, bits - 1)));
}

class SerialTwoWireStream {
public:
    using size_type = uint16_t;

    static constexpr size_type kAllocMinSize = I2C_OVER_UART_ALLOC_MIN_SIZE;
    static constexpr size_type kAllocBlockSize = I2C_OVER_UART_ALLOC_BLOCK_SIZE;
    static constexpr size_type kAllocBlockBitMask = __constexpr_is_bitmask(kAllocBlockSize) ? (kAllocBlockSize - 1) : 0;

    SerialTwoWireStream(const SerialTwoWireStream &) = delete;
    SerialTwoWireStream &operator=(const SerialTwoWireStream &) = delete;
    SerialTwoWireStream(SerialTwoWireStream &&move) = delete;
    SerialTwoWireStream &operator=(SerialTwoWireStream &&move) = delete;

    SerialTwoWireStream();
    ~SerialTwoWireStream();

    void setAllocMinSize(uint8_t size);

    void clear();
    void release();

    int read();
    int peek();
    uint8_t charAt(int offset) const;

    uint8_t operator[](int offset) const;
    uint8_t &operator[](int offset);

    size_type write(uint8_t data);
    size_type write(const uint8_t *data, size_t len);

    size_type length() const;
    size_type size() const;
    size_t available() const;
    bool empty() const;

    size_type push_back(uint8_t data);      // write

    int pop_back();                         // undo write
    int pop_front();                        // read

    uint8_t *begin();
    uint8_t *end();

    size_type read(uint8_t *data, size_t length);
    size_t readBytes(uint8_t *data, size_t len);

    bool reserve(size_type new_size);

private:
    bool resize(size_type size);
    size_type _resize(size_type size);
    size_type _get_block_size(size_type new_size) const;

    uint8_t *_buffer;
    size_type _length;
    size_type _size;
    size_type _position;
    uint8_t _allocMinSize;
};

#include "SerialTwoWireStream.hpp"

#elif SERIALTWOWIRE_USE_BUFFERSTREAM

#include <BufferStream.h>

class SerialTwoWireStream : public BufferStream
{
public:
    using BufferStream::BufferStream;

    // Buffer uses an alloc block size of 8
    // remove contents and resize buffer to minimum
    inline void clear() {
        // remove all data
        BufferStream::reset();
        Buffer::shrink(kAllocMinSize);
    }

    inline uint8_t *begin() {
        return Buffer::begin() + position();
    }

    // release all memory
    inline void release() {
        BufferStream::clear();
    }

    inline size_t available() const {
        return BufferStream::_available();
    }

    inline void shrink_to_fit() {
        // remove data that has been read and resize buffer to max. length + 15
        BufferStream::removeAndShrink(0, position(), kAllocMinSize);
    }

    inline void setAllocMinSize(uint8_t size) {}
};

#else

#include <StreamString.h>

class SerialTwoWireStream : public StreamString
{
public:
    using StreamString::StreamString;

    // remove contents and resize buffer to minimum
    inline void clear() {
        String::remove(0, ~0);
        shrink_to_fit();
    }

    inline uint8_t *begin() {
        return reinterpret_cast<uint8_t *>(begin());
    }

    inline uint8_t *end() {
        return reinterpret_cast<uint8_t *>(begin() + length());
    }

    // release all memory
    inline void release() {
        String::invalidate();
    }

    inline void shrink_to_fit() {
        if (capacity > String::length() + kAllocMinSize) {
            // keep buffer above minimum otherwise the String might free the memory
            size_t size = (String::length() < kAllocMinSize) ? kAllocMinSize : String::length();
            String::changeBuffer(size);
        }
    }

    inline void setAllocMinSize(uint8_t size) {}
};

#endif
