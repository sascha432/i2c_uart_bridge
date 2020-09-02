/**
 * Author: sascha_lammers@gmx.de
 */

//
// Extension to match functionality and method names
//

#if SERIALTWOWIRE_USE_BUFFERSTREAM

#include <BufferStream.h>

class SerialTwoWireStream : public BufferStream
{
public:
    using BufferStream::BufferStream;

    // keep the buffer size above 15 byte to reduce realloc calls
    // Buffer uses a 8 byte block size, having recalloc only called to change to 16, 24, 32, ... bytes
    static constexpr size_t kAllocMinSize = 15;

    // remove contents and resize buffer to minimum
    inline void clear() {
        // remove all data
        BufferStream::reset();
        Buffer::shrink(15);
    }

    // release all memory
    inline void release() {
        BufferStream::clear();
    }

    inline void shrink_to_fit() {
        // remove data that has been read and resize buffer to max. length + 15
        BufferStream::removeAndShrink(0, position(), 15);
    }
};

#else

#include <StreamString.h>

class SerialTwoWireStream : public StreamString
{
public:
    using StreamString::StreamString;

#if __AVR__
    // keep buffer > 8 bytes to reduce alloc calls
    // the block size is 4, realloc will be called for 8, 12, 16, ... bytes
    static constexpr size_t kAllocMinSize = 7;
    static constexpr size_t kAllocBlockSize = 4;
#elif defined(ESP8266) || defined(ESP32)
    // keep the buffer size above 15 byte to reduce realloc calls
    // String for ESP8266 uses a 16 byte block size having realloc called for 16, 32, 48, .. bytes
    static constexpr size_t kAllocMinSize = 15;
    static constexpr size_t kAllocBlockSize = 0;
#else
    static constexpr size_t kAllocMinSize = 15;
    static constexpr size_t kAllocBlockSize = 8;
#endif

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
            size_t size = (String::length() < kAllocMinSize) ? kAllocMinSize : String::length();
            if (kAllocBlockSize) {
                size = (size + (kAllocBlockSize - 1)) & ~(kAllocBlockSize - 1);
            }
            // keep buffer above minimum otherwise the String might free the memory
            String::changeBuffer(size);
        }
    }
};

#endif
