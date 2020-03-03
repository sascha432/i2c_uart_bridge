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

### Master

The master can send data to slaves and request data from slaves by writing to the serial port.

#### Sending data

+I2CT=\<address\>,\<data\>[,\<data\>[,...]]

Address and data are in 2 digit hex format. The comma is for readablity and optional.

#### Requesting data

+I2CR=\<address\>,\<length\>

Address and length are in 2 digit hex format.

#### Receiving from slaves

Data from slaves is read from the serial port.

+I2CT=\<address\>\<data\>[\<data\>[...]]

### Scanning the I2C bus

+I2CS
