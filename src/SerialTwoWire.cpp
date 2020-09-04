/**
 * Author: sascha_lammers@gmx.de
 */

#include "SerialTwoWire.h"
#include "SerialTwoWireDebug.h"

#ifndef SERIALTWOWIRE_NO_GLOBALS

SerialTwoWire Wire;

void serialEvent()
{
    Wire._serialEvent();
}

#endif

const char SerialTwoWireDef::transmitCommand[] PROGMEM = { I2C_OVER_UART_PREFIX_TRANSMIT };
const char SerialTwoWireDef::requestCommand[] PROGMEM = { I2C_OVER_UART_PREFIX_REQUEST };

