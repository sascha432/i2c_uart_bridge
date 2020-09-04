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

constexpr bool __constexpr_is_bitmask(size_t num, size_t bits = 31) {
    return bits == ~0U ? false : (num == (1U << bits) ? true : __constexpr_is_bitmask(num, bits - 1));
}

class SerialTwoWireStream {
public:
    using size_type = uint16_t;

    static constexpr size_type kAllocMinSize = I2C_OVER_UART_ALLOC_MIN_SIZE;
    static constexpr size_type kAllocBlockSize = I2C_OVER_UART_ALLOC_BLOCK_SIZE;
    static constexpr size_type kAlllocBlockBitMask = __constexpr_is_bitmask(kAllocBlockSize) ? (kAllocBlockSize - 1) : 0;

    SerialTwoWireStream() : _buffer(nullptr), _length(0), _size(0), _position(0) {}
    ~SerialTwoWireStream() {
        release();
    }

    void clear();
    void release();

    inline int read() {
        if (_position < _length) {
            return _buffer[_position++];
        }
        return -1;
    }

    inline int peek() {
        if (_position < _length) {
            return _buffer[_position];
        }
        return -1;
    }

    inline uint8_t charAt(int offset) const {
        if ((size_t)offset >= _length) {
            return 0;
        }
        return _buffer[offset];
    }

    inline uint8_t operator[](int offset) const {
        return _buffer[offset];
    }

    inline uint8_t &operator[](int offset) {
        return _buffer[offset];
    }

    inline size_type write(uint8_t data) {
        if (_length < _size) {
            _buffer[_length++] = data;
        }
        else {
            if (!resize(_length + 1)) {
                release();
                return 0;
            }
            _buffer[_length++] = data;
        }
        return 1;
    }

    inline size_type write(const uint8_t *data, size_type len) {
        size_type required = len + _length;
        if (required >= _size) {
            if (!resize(required + 1)) {
                return 0;
            }
        }
        std::copy_n(data, len, &_buffer[_length]);
        _length += len;
        return len;
    }

    inline size_type length() const {
        return _length;
    }

    inline size_type size() const {
        return _size;
    }

    inline size_t available() const {
        return _length - _position;
    }

    inline bool empty() const {
        return _length == _position;
    }

    inline size_type push_back(uint8_t data) {
        return write(data);
    }

    inline int pop_back() {
        if (_length > 0) {
            if (_position >= _length) {
                _position--;
            }
            return _buffer[--_length];
        }
        return -1;
    }

    inline int pop_front() {
        return read();
    }

    uint8_t *begin() {
        return &_buffer[_position];
    }

    uint8_t *end() {
        return &_buffer[_length];
    }

    inline size_type read(uint8_t *data, size_type len) {
        size_type avail = _length - _position;
        if (len > avail) {
            len = avail;
        }
        std::copy_n(begin(), len, data);
        _position += len;
        return len;
    }

    inline size_t readBytes(uint8_t *data, size_t len) {
        return read(data, (size_type)len);
    }

    inline bool reserve(size_type new_size) {
        return new_size <= _size ? true : resize(new_size);
    }

private:
    bool resize(size_type size);
    size_type _resize(size_type size);
    size_type _get_block_size(size_type new_size) const;

    uint8_t *_buffer;
    size_type _length;
    size_type _size;
    size_type _position;
};

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
};

#endif
