# I2C UART bridge

This class can be used as drop in replacement for the TwoWire class, using the I2C protocol over UART. Both master and slaves replace TwoWire with SerialTwoWire using any Stream object (HardwareSerial, SoftwareSerial, TCP2Serial, WebSockets, ...)

It supports master-slave and master-master communication.

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

### Sending data

+I2CT=\<address\>,\<data\>[,\<data\>[,...]]\<LF\>

Master and slave are transmitting data to the serial port.

#### Requesting data

+I2CR=\<address\>,\<length\>\<LF\>

The master sends requests to the serial port.

#### Receiving from slaves

+I2CT=\<address\>\<data\>[\<data\>[...]]\<LF\>

Master and slave are reading transmissions from the serial port.

## Concurrency and collisions

### Locking and acknowledgement

Locking and acknowledgement is currently not implemented. The serial communication is based in 2 wires, without any other signal like DTS, RTS etc... which would required additonal serial communication and cause a lot overhead and delays. This might lead to collisions in master/master mode if both masters are sending data at the same time. The data becomes invalid and both transmissions are discarded.

### Collisions

A master can request data and receive transmissions at the same time without collissions. If a slave responds after the master aborted the request cause of a timeout and requesting data from another slave, this might lead to a collison and both transmissions would be discarded. These problems only occur with low serial bandwith and high concurrency.

I suggest to implement random timeouts, delays and retries on the master side, which should sufficiently mitigate the issue.
