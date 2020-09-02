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

const char _i2c_transmit_cmd[] PROGMEM = { I2C_OVER_UART_PREFIX_TRANSMIT };
const char _i2c_request_cmd[] PROGMEM = { I2C_OVER_UART_PREFIX_REQUEST };

