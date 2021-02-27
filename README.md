# I2C UART bridge

This class can be used as drop in replacement for the TwoWire class, using the I2C protocol over UART. Both master and slaves replace TwoWire with SerialTwoWire using any Stream object (HardwareSerial, SoftwareSerial, TCP2Serial, WebSockets, ...)

It supports master-slave and master-master communication.

## Platforms

Supported platforms are atmelavr, espressif8266 and espressif32. Visual Studio 2019 can be used to compile native Win32 applications. This requires an Arduino library that compiles for this target.

## Changel log

[Change Log v0.2.0](CHANGELOG.md)

## Example of serial communication

    SoftwareSerial SoftSerial(8, 9);
    SerialTwoWire Wire(SoftSerial);

    Tcp2Serial TcpServerSerial(Tcp2Serial::SERVER, 22); // listen on port 22
    SerialTwoWire Wire(TcpServerSerial);

    SerialTwoWire Wire; // use Serial

    Wire.beginTransmission(0x17);
    Wire.write(0x89);
    Wire.write(0x22);
    if (Wire.endTransmission() == 0) { // Sends "+I2CT=178922\n"

        uint16_t word;
        if (Wire.requestFrom(0x17, 2) == 2) {   // Sends "+I2CR=1702\n"
            // The response is "+I2CT=173412\n"
            Wire.readBytes(reinterpret_cast<const uint8_t *>(&word), sizeof(word)); // word=0x1234
        }
    }

## I2C USB2Serial bridge

I2C Master example for Microsoft Visual Studio Win32 using the UART bridge to communicate over USB with an Arduino Nano and I2C deviced connected to it.

LM75A, 16x2 LCD display examples

## I2C WiFi bridge

If you want to use WiFi to instead of USB to communicate with the I2C bus, the ESP8266 firmware [esp-link](https://github.com/jeelabs/esp-link) can be used to communicate with the Arduino.

esp-link is a transparent bridge between Wifi and serial.

## I2C WiFi bridge without Ardunio

[KFC Firmware](https://github.com/sascha432/esp8266-kfc-fw) supports I2C over web sockets with optional web interface (Http2Serial) and TCP (TCP2Serial) using the UART bridge.

## Protocol

The master can send data to slaves and request data from slaves by writing to the serial port.

## Transmissions and data encoding

Data is sent as 2 digit hex value. Whitespace is allowed to separate bytes.
Allowed charaters are 0-9, a-f, A-F, comma, tab and space.
The end of a transmission is makred by a line feed. carriage return is optional.
The maximum transmission length is 254 byte, including the I2C address.

### Transmitting data to slaves

+I2CT=\<address\>,\<data\>[,\<data\>[,...]]\<LF\>

A Master is transmitting data for the slave with \<address\> to the serial port.

#### Requesting data from slaves

+I2CR=\<address\>,\<length\>\<LF\>

A master is requesting \<length\> bytes from the slave \<address\>.

#### Receiving responses from slaves

+I2CA=\<address\>\<data\>[\<data\>[...]]\<LF\>

The slave with \<address\> is sending a response to the serial port.

#### Additional output

Master and slave might send additional information using the REM command

+REM=...

## Concurrency and collisions

### Locking and acknowledgement

Locking and acknowledgement is currently not implemented. The serial communication is based on 2 wires, without any other signal like RTS, CTS, DTR... This might lead to collisions in master/master mode if both masters are sending data at the same time. The data becomes invalid and both transmissions are discarded.

### Collisions

A master can request data and receive transmissions at the same time without collissions. If a slave responds after the master aborted the request cause of a timeout and requesting data from another slave, this might lead to a collison and both transmissions would be discarded. These problems only occur with low serial bandwidth and high concurrency.

My suggestion is to implement random timeouts, delays and retries on the master side, which should sufficiently mitigate the issue. An optional checksum can ensure data integrity.

### Sharing the serial port with multiple devices

The signal between devices should be TTL level with a single converter to RS232, if required. To avoid shorts and high currents, different TX pins should not be connected directly together but with a 1-10KOhm resistor. Idle master and slaves should set the TX pin to a high impedance state or using the internal pullup resistor (i.e. ATmega328p 20-50kOhm).

Another option for devices that cannot set TX to a high impedance state, is to use several input pins for TX and trigger an interrupt on level change. As long as one of the input pins is low, the TX pin will be set to low as well. A resistor in series to protect the input pins is recommended.

If a lot other data is transferred over the port serial port (like debug messages) it is recommended to enable the checksum to avoid corrupted data.
