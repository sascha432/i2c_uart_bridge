/**
 * Author: sascha_lammers@gmx.de
 */

// I2C over UART emulation

#pragma once

#include <Arduino.h>

#if DEBUG_SERIALTWOWIRE
#include <debug_helper.h>
#include <debug_helper_enable.h>
#endif

class Serial;

namespace SerialTwoWireDef {

    #ifndef DEBUG_SERIALTWOWIRE
    #define DEBUG_SERIALTWOWIRE                     0
    #endif

    // increases serial data by 4 byte, required buffer length by 2 and increases processing time
    #ifndef I2C_OVER_UART_ADD_CRC16
    #define I2C_OVER_UART_ADD_CRC16                 0
    #endif

    // needs quite some memory
    #ifndef I2C_OVER_UART_USE_STD_FUNCTION
    #define I2C_OVER_UART_USE_STD_FUNCTION          0
    #endif
    #if __AVR__ && I2C_OVER_UART_USE_STD_FUNCTION
    #error check if stl support is available
    #endif

    // possible values are 511 or 65535
    // I2C is limited to 255 bytes + kCommandBufferSize (7 by default)
    #ifndef SERIALTWOWIRE_STREAM_CLASS_MAX_LEN
    #define SERIALTWOWIRE_STREAM_CLASS_MAX_LEN      511
    #endif

    #if I2C_OVER_UART_ADD_CRC16
    static constexpr uint8_t kRequestTransmissionMaxLength = 2 + sizeof(uint16_t);
    static constexpr uint8_t kCrcStartChar = '#';
    #else
    static constexpr uint8_t kRequestTransmissionMaxLength = 2;
    #endif

    // set to 0 if using I2C slave mode only
    // set to 1 if using master or slave and master
    #ifndef I2C_OVER_UART_ENABLE_MASTER
    #define I2C_OVER_UART_ENABLE_MASTER             1
    #endif

    // must start with +
    #ifndef I2C_OVER_UART_PREFIX_TRANSMIT
    #define I2C_OVER_UART_PREFIX_TRANSMIT           "+I2CT="
    #endif

    #ifndef I2C_OVER_UART_PREFIX_REQUEST
    #define I2C_OVER_UART_PREFIX_REQUEST            "+I2CR="
    #endif

    // any command that starts with '+' but does match I2C_OVER_UART_PREFIX_TRANSMIT or
    // I2C_OVER_UART_PREFIX_REQUEST will be forwarded to the onReceived callback.
    // set to 0 to disable. any other number will limit the data that is collected
    #ifndef I2C_OVER_UART_ENABLE_DISCARD_COMMAND
    #define I2C_OVER_UART_ENABLE_DISCARD_COMMAND    0
    //#define I2C_OVER_UART_ENABLE_DISCARD_COMMAND    19 // collect up to 19 byte
    #endif

    // first byte to identify discarded transmissions (onReceive callback)
    // if '+' is used, it won't added since the command starts with '+' already
    #ifndef I2C_OVER_UART_ENABLE_DISCARD_ID
    #define I2C_OVER_UART_ENABLE_DISCARD_ID         43 // '+'
    //#define I2C_OVER_UART_ENABLE_DISCARD_ID         0xff
    #endif

    // maximum binary input length before discarding incoming messages
    #ifndef I2C_OVER_UART_MAX_INPUT_LENGTH
    #define I2C_OVER_UART_MAX_INPUT_LENGTH          255
    #endif

    static_assert(I2C_OVER_UART_ENABLE_DISCARD_COMMAND < I2C_OVER_UART_MAX_INPUT_LENGTH - 1, "maximum length exceeded");

    static constexpr size_t kTransmissionMaxLength = I2C_OVER_UART_MAX_INPUT_LENGTH;
    static_assert(kTransmissionMaxLength <= 255, "maximum length exceeded");

    // I2C_OVER_UART_ALLOC_MIN_SIZE is the minimum size of the send and receive buffers
    // set to 0 to release the memory after each transmission. this works well for reading
    // sensors every few seconds or even minutes
    //
    // set above the length of frequent transmissions to reduce the number of reallocations
    // every transmission that exceeds the min. size will call realloc at least twice
    // the I2C address adds one extra byte to the actual data length
    //
    // every transmission is collected, stored in the buffer, prepared and when the
    // serial port is ready, sent at once and flushed.
    // when sending content to a tft display, it might be necessary to by-pass the buffer
    // and send the directly to the serial port to avoid double buffering
    //
    // increasing the minimum above I2C_OVER_UART_MAX_INPUT_LENGTH ensures that the buffers
    // won't be reallocated after until end() is called
    //
    // if the serial port is used for other purposes like debugging messages, setting
    // I2C_OVER_UART_ALLOC_MIN_SIZE to kCommandBufferSize ensures that messages
    // that are not for I2C wont produce any reallocations
    //
    // I2C_OVER_UART_ALLOC_BLOCK_SIZE is the number of bytes that will the allocated at
    // once if one byte more space is required. this value should match the allocators
    // blocksize one or multiple times
    //
    #ifndef I2C_OVER_UART_ALLOC_MIN_SIZE
    #if __AVR__
    #define I2C_OVER_UART_ALLOC_MIN_SIZE            0
    #define I2C_OVER_UART_ALLOC_BLOCK_SIZE          8
    #elif defined(ESP8266) || defined(ESP32)
    #define I2C_OVER_UART_ALLOC_MIN_SIZE            0
    #define I2C_OVER_UART_ALLOC_BLOCK_SIZE          16
    #else
    #define I2C_OVER_UART_ALLOC_MIN_SIZE            7
    #define I2C_OVER_UART_ALLOC_BLOCK_SIZE          16
    #endif
    #endif

    #ifndef FPSTR
    #define FPSTR(str) reinterpret_cast<const __FlashStringHelper *>(str)
    #endif

    #if __GCC__

    constexpr size_t __constexpr_strlen(const char *s) noexcept {
        return __builtin_strlen(s);
    }

    #else

    constexpr size_t __constexpr_strlen(const char *s) noexcept {
        return *s ? 1 + __constexpr_strlen(s + 1) : 0;
    }

    #endif

    extern const char transmitCommand[] PROGMEM;

    static constexpr size_t kTransmitCommandLength = __constexpr_strlen(I2C_OVER_UART_PREFIX_TRANSMIT);

    #if I2C_OVER_UART_ENABLE_MASTER

    extern const char requestCommand[] PROGMEM;

    static constexpr size_t kRequestCommandLength = __constexpr_strlen(I2C_OVER_UART_PREFIX_REQUEST);

    #else

    static constexpr size_t kRequestCommandLength = 0;

    #endif

    #define I2C_OVER_UART_MAX_TRANS_CMD_LEN (kTransmitCommandLength > kRequestCommandLength ? kTransmitCommandLength : kRequestCommandLength)

    static constexpr uint8_t kCommandMaxLength = I2C_OVER_UART_ENABLE_DISCARD_COMMAND > I2C_OVER_UART_MAX_TRANS_CMD_LEN ? I2C_OVER_UART_ENABLE_DISCARD_COMMAND : I2C_OVER_UART_MAX_TRANS_CMD_LEN;
    static_assert(kCommandMaxLength >= 4, "min. size required");

}

#if DEBUG_SERIALTWOWIRE
#include <debug_helper_disable.h>
#endif
