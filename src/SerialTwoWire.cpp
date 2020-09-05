/**
 * Author: sascha_lammers@gmx.de
 */

#include "SerialTwoWire.h"
#include "SerialTwoWireSlave.h"
#include "SerialTwoWireDebug.h"

#ifndef SERIALTWOWIRE_NO_GLOBALS

SerialTwoWire Wire;

void serialEvent()
{
    auto &serial = Wire.getSerial();
    while (serial.isAvailable()) {
        Wire.feed(serial.read());
    }
}

#endif

const char SerialTwoWireDef::transmitCommand[] PROGMEM = { I2C_OVER_UART_PREFIX_TRANSMIT };
const char SerialTwoWireDef::requestCommand[] PROGMEM = { I2C_OVER_UART_PREFIX_REQUEST };

