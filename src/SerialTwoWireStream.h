/**
 * Author: sascha_lammers@gmx.de
 */

//
// Extension to match functionality and method names
//

#include "SerialTwoWireDef.h"

#if I2C_OVER_UART_ADD_CRC16
#include <crc16.h>
#endif

#if SERIALTWOWIRE_USE_BUFFERSTREAM

#include <BufferStream.h>

class SerialTwoWireStream : public BufferStream
{
public:
    using BufferStream::BufferStream;

    // Buffer uses an alloc block size of 8
    static constexpr size_t kAllocMinSize = I2C_OVER_UART_ALLOC_MIN_SIZE;

    // remove contents and resize buffer to minimum
    inline void clear() {
        // remove all data
        BufferStream::reset();
        Buffer::shrink(kAllocMinSize);
    }

    // release all memory
    inline void release() {
        BufferStream::clear();
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

    // some String implementations might free if kAllocMinSize is below a certain value
    // the allocation block size depends on the implementation as well
    // ESP8266 < 11 = free dynamic memory, alloc block size 16
    static constexpr size_t kAllocMinSize = I2C_OVER_UART_ALLOC_MIN_SIZE;

    // remove contents and resize buffer to minimum
    inline void clear() {
        String::remove(0, ~0);
        shrink_to_fit();
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

// #if I2C_OVER_UART_ADD_CRC16

// class SerialTwoWireCrcStream : public SerialTwoWireStream {
// public:
//     inline void clear() {
//         _crc = ~0;
//         SerialTwoWireStream::clear();
//     }

//     inline void writeAddress(uint8_t address) {
//         _crc = ~0;
//         writeData(address);
//     }

//     inline void writeData(uint8_t data) {
//         _crc = _crc16_update(_crc, data);
//         write(data);
//     }

//     // inline void finalize() {
//     //     write((uint8_t)_crc);
//     //     write(_crc >> 8);
//     // }



// // private:
// //     inline void _writeNibble(uint8_t nibble) {
// //         write(nibble < 0xa ? nibble : nibble + ('a' - 0xa))
// //     }

// protected:
//     uint16_t _crc;
// };

// #else

// class SerialTwoWireCrcStream : public SerialTwoWireStream {
// public:
//     inline void writeAddress(uint8_t address) {
//         write(address);
//     }

//     inline void writeData(uint8_t data) {
//         write(data);
//     }

//     constexpr bool isValid() {
//         return true;
//     }
// };

// #endif