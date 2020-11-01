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

#if DEBUG_SERIALTWOWIRE
#include <debug_helper.h>
#include <debug_helper_enable.h>
#endif

using namespace SerialTwoWireDef;

//
// Compared to StreamString:
// smaller, faster, better dynamic memory management and requires much less memory
//
// static constexpr size_t SerialTwoWireStreamSize = sizeof(SerialTwoWireStream);          //  8 byte
// static constexpr size_t BufferStreamSize = sizeof(BufferStream);                        // 44 byte
// static constexpr size_t StreamStringSize = sizeof(StreamString);                        // 52 byte

constexpr bool __constexpr_is_bitmask(size_t num, uint16_t bits = 15) {
    return num == 0U || bits == 0U ? false : ((num == (1U << bits) ? true : __constexpr_is_bitmask(num, bits - 1)));
}

class SerialTwoWireStream {
public:
    using size_type = uint16_t;

    static constexpr size_type kAllocMinSize = I2C_OVER_UART_ALLOC_MIN_SIZE;
    static constexpr size_type kAllocBlockSize = I2C_OVER_UART_ALLOC_BLOCK_SIZE;
    static constexpr size_type kAllocBlockBitMask = __constexpr_is_bitmask(kAllocBlockSize) ? (kAllocBlockSize - 1) : 0;

#if SERIALTWOWIRE_STREAM_CLASS_MAX_LEN < 512
    static constexpr size_type kBitsSize = 9;
    static constexpr size_type kBitsMinAlloc = 5;
#else
    static constexpr size_type kBitsSize = sizeof(size_type) * 8;
    static constexpr size_type kBitsMinAlloc = kBitsSize;
#endif

    SerialTwoWireStream(const SerialTwoWireStream &) = delete;
    SerialTwoWireStream &operator=(const SerialTwoWireStream &) = delete;
    SerialTwoWireStream(SerialTwoWireStream &&move) = delete;
    SerialTwoWireStream &operator=(SerialTwoWireStream &&move) = delete;

    SerialTwoWireStream();
    ~SerialTwoWireStream();

    // limited from 0 to kMinAllocSizeLimit
    // set to kMinAllocNoRealloc to disable changing buffer size
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
    static constexpr size_t max_length() {
        return (1 << kBitsSize) - 1;
    }
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
    size_type _get_alloc_min_size() const;

    uint8_t *_buffer;
#if SERIALTWOWIRE_STREAM_CLASS_MAX_LEN < 512
    uint32_t _length: kBitsSize;
    uint32_t _position : kBitsSize;
    uint32_t _size: kBitsSize;
    uint32_t _allocMinSize: kBitsMinAlloc;
#else
    size_type _length;
    size_type _position;
    size_type _size;
    size_type _allocMinSize;
#endif
};

#include "SerialTwoWireStream.hpp"

static constexpr size_t SerialTwoWireStreamSize = sizeof(SerialTwoWireStream);

#if DEBUG_SERIALTWOWIRE
#include <debug_helper_disable.h>
#endif
