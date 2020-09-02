/**
 * Author: sascha_lammers@gmx.de
 */

// I2C over UART emulation

#pragma once

#ifndef DEBUG_SERIALTWOWIRE
#define DEBUG_SERIALTWOWIRE                     0
#endif

// set to 0 if using I2C slave mode only
// set to 1 if using master or slave and master
#ifndef I2C_OVER_UART_ENABLE_MASTER
#define I2C_OVER_UART_ENABLE_MASTER             1
#endif

#ifndef I2C_OVER_UART_PREFIX_TRANSMIT
#define I2C_OVER_UART_PREFIX_TRANSMIT           "+I2CT="
#endif

#ifndef I2C_OVER_UART_PREFIX_REQUEST
#define I2C_OVER_UART_PREFIX_REQUEST            "+I2CR="
#endif

// maximum binary input length, 0 to disable
#ifndef I2C_OVER_UART_MAX_INPUT_LENGTH
#define I2C_OVER_UART_MAX_INPUT_LENGTH          254
#endif

static_assert(I2C_OVER_UART_MAX_INPUT_LENGTH < 255, "maximum excceeded");


#if _MSC_VER
#define ___max_str_len(a, b) (constexpr_strlen(a) > constexpr_strlen(b) ? constexpr_strlen(a) : constexpr_strlen(b))
#else
#define ___max_str_len(a, b) (__builtin_strlen(a) > __builtin_strlen(b) ? __builtin_strlen(a) : __builtin_strlen(b))
#endif

class Serial;

extern const char _i2c_transmit_cmd[] PROGMEM;
extern const char _i2c_request_cmd[] PROGMEM;
