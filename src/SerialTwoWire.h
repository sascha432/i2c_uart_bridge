/**
 * Author: sascha_lammers@gmx.de
 */

// I2C over UART emulation

#pragma once

#include <Arduino.h>
#include "SerialTwoWireDef.h"
#include "SerialTwoWireSlave.h"

#if I2C_OVER_UART_MODE == I2C_OVER_UART_MODE_MASTER
#include "SerialTwoWireMaster.h"
using SerialTwoWire = SerialTwoWireMaster;
#else
using SerialTwoWire = SerialTwoWireSlave;
#endif

#if DEBUG_SERIALTWOWIRE
#include <debug_helper.h>
#include <debug_helper_enable.h>
#endif

#if defined(ESP8266) || defined(ESP32)

#elif __AVR__

static inline bool can_yield() {
    return true;
}

static inline void optimistic_yield(uint32_t) {
    yield();
}

#elif _MSC_VER

#include <thread>

static inline bool can_yield() {
    return true;
}

static inline void optimistic_yield(uint32_t) {
    std::this_thread::yield();
}

#endif

#ifndef SERIALTWOWIRE_NO_GLOBALS

void serialEvent();

extern SerialTwoWire Wire;

#endif

#if DEBUG_SERIALTWOWIRE
#include <debug_helper_disable.h>
#endif
