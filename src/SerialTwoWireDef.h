/**
 * Author: sascha_lammers@gmx.de
 */

// I2C over UART emulation

#pragma once

#include <Arduino.h>

#ifndef DEBUG_SERIALTWOWIRE
#define DEBUG_SERIALTWOWIRE                     0
#endif

// increases serial data by 4 byte, required buffer length by 2 and processing time
#ifndef I2C_OVER_UART_ADD_CRC16
#define I2C_OVER_UART_ADD_CRC16                 1
#endif

#define I2C_OVER_UART_CRC_START                 '#'

#if I2C_OVER_UART_ADD_CRC16
#define I2C_OVER_UART_REQUEST_COMMAND_LENGTH    (2 + sizeof(uint16_t))
#else
#define I2C_OVER_UART_REQUEST_COMMAND_LENGTH    2
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

// maximum binary input length before discarding a message
#ifndef I2C_OVER_UART_MAX_INPUT_LENGTH
#define I2C_OVER_UART_MAX_INPUT_LENGTH          254
#endif

static_assert(I2C_OVER_UART_MAX_INPUT_LENGTH < 255, "maximum exceeded");

#ifndef I2C_OVER_UART_ALLOC_MIN_SIZE
#if __AVR__
#define I2C_OVER_UART_ALLOC_MIN_SIZE            0
#elif defined(ESP8266) || defined(ESP32)
#define I2C_OVER_UART_ALLOC_MIN_SIZE            15
#else
// minimum size of the receive buffers. set to 0 to free all memory
// set above the length of frequent transimssions to reduce the number of reallocations
// every transmission that exceeds the min size will call realloc at least twice
// the I2C address adds one extra byte to the actual data length
// increasing the minimum above I2C_OVER_UART_MAX_INPUT_LENGTH ensures that the buffers
// won't be reallocated
#define I2C_OVER_UART_ALLOC_MIN_SIZE            15
#endif
#endif

class Serial;

#if __GCC__

constexpr size_t __constexpr_strlen(const char *s) noexcept {
    return __builtin_strlen(s);
}

#else

constexpr size_t __constexpr_strlen(const char *s) noexcept {
    return *s ? 1 + __constexpr_strlen(s + 1) : 0;
}

#endif

extern const char _i2c_transmit_cmd[] PROGMEM;

static constexpr size_t kSerialTwoWireTransmitCommandLength = __constexpr_strlen(I2C_OVER_UART_PREFIX_TRANSMIT);

#if I2C_OVER_UART_ENABLE_MASTER

extern const char _i2c_request_cmd[] PROGMEM;

static constexpr size_t kSerialTwoWireRequestCommandLength = __constexpr_strlen(I2C_OVER_UART_PREFIX_REQUEST);

#else

static constexpr size_t kSerialTwoWireRequestCommandLength = 0;

#endif

static constexpr size_t kSerialTwoWireMaxCommandLength = kSerialTwoWireTransmitCommandLength > kSerialTwoWireTransmitCommandLength ? kSerialTwoWireTransmitCommandLength : kSerialTwoWireTransmitCommandLength;

static_assert(kSerialTwoWireMaxCommandLength >= 4, "min. size required");
