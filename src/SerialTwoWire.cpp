/**
 * Author: sascha_lammers@gmx.de
 */

#include "SerialTwoWire.h"
#include "SerialTwoWireSlave.h"
#include "SerialTwoWireDebug.h"

#if DEBUG_SERIALTWOWIRE
#include <debug_helper.h>
#include <debug_helper_enable.h>
#endif

#ifndef SERIALTWOWIRE_NO_GLOBALS

SerialTwoWire Wire;

void serialEvent()
{
    auto &serial = Wire.getSerial();
    while (serial.available()) {
        Wire.feed(serial.read());
    }
}

#endif

const char SerialTwoWireDef::transmitCommand[] PROGMEM = { I2C_OVER_UART_PREFIX_TRANSMIT };
const char SerialTwoWireDef::requestCommand[] PROGMEM = { I2C_OVER_UART_PREFIX_REQUEST };

