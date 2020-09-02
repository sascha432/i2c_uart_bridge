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

    // avoid excessive allocations
    inline void clear() {
        removeAndShrink();
    }

    // release all memory
    inline void release() {
        BufferStream::clear();
    }

    inline void removeAndShrink() {
        // remove data that has been read and shrink to fit, leaving current size + 16 byte allocated to reduce memory ops
        BufferStream::removeAndShrink(position(), ~0, 16);
    }
};

#else

#include <StreamString.h>

class SerialTwoWireStream : public StreamString
{
public:
    using StreamString::StreamString;

    // avoid excessive allocations
    inline void clear() {
        removeAndShrink();
    }

    // release all memory
    inline void release() {
        String::invalidate();
    }

    inline void removeAndShrink() {
        // StreamString does not keep data that has been read
        // resize to current length + 15
        if (capacity > String::length() + 15) {
            String::changeBuffer(String::length() + 15);
        }
    }
};

#endif
