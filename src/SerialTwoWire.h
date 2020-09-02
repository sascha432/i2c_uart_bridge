/**
 * Author: sascha_lammers@gmx.de
 */

// I2C over UART emulation

#pragma once

#include <Arduino.h>
#include "SerialTwoWireDef.h"
#include "SerialTwoWireSlave.h"

#if I2C_OVER_UART_ENABLE_MASTER
#include "SerialTwoWireMaster.h"
using SerialTwoWire = SerialTwoWireMaster;
#else
using SerialTwoWire = SerialTwoWireSlave;
#endif

#ifndef SERIALTWOWIRE_NO_GLOBALS

void serialEvent();

extern SerialTwoWire Wire;

#endif
