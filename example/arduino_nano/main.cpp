/**
  Author: sascha_lammers@gmx.de
*/

// I2C master
// Flash on an Arduino Nano, add a 16x2 LCD to the I2C bus (or any other device) and run the windows app

#include <Arduino.h>
#include <SerialTwoWire.h>

void scan_i2c_bus();
uint8_t *parse_data(const char *data, size_t hex_length, size_t &length);

void setup()
{
    Serial.begin(115200);
#if I2C_OVER_UART_ENABLE_MASTER
    Wire.begin();
#else
    Wire.begin(0x01);
#endif
    Serial.println(F("+PING"));
}

String line;

void loop()
{
    if (Serial.available()) {
        char tmp = Serial.read();
        if (tmp == '\n') {
            size_t length = 0;
            uint8_t *data = nullptr;
            if (strcasecmp_P(line.c_str(), PSTR("+PING")) == 0) { // return pong
                Serial.println(F("+PONG"));
            }
            else if (strcasecmp_P(line.c_str(), PSTR("+I2CS")) == 0) { // scan
                scan_i2c_bus();
            }
            else if (strncasecmp_P(line.c_str(), PSTR("+I2CT="), 6) == 0) {     // transmit to Wire from Serial
                auto data = parse_data(line.c_str() + 6, line.length() - 6, length);
                if (data && length) {
                    Wire.beginTransmission(*data);
                    Wire.write(data, length - 1);
                    Wire.endTransmission();
                    // auto result = Wire.endTransmission();
                    // Serial.print(F("+I2CE="));
                    // Serial.println(result);
                }
            } else if (strncasecmp_P(line.c_str(), PSTR("+I2CR="), 6) == 0) {   /// request from Wire and transmit to Serial
                auto data = parse_data(line.c_str() + 6, line.length() - 6, length);
                if (data && length >= 2) {
                    char buf[4];
                    uint8_t request_length = *(data + 1);
                    Serial.print(F("+I2CT="));
                    snprintf_P(buf, sizeof(buf) - 1, PSTR("%02x"), *data);
                    Serial.print(buf);
#if I2C_OVER_UART_ENABLE_MASTER
                    if (Wire.requestFrom(*data, request_length) == request_length) {
                        while(Wire.available()) {
                            snprintf_P(buf, sizeof(buf) - 1, PSTR("%02x"), Wire.read());
                            Serial.print(buf);
                        }
                    }
#endif
                    Serial.println();
                }
            }
            if (data) {
                free(data);
            }
            line = String();
        }
        else if (tmp == '\r') {
        }
        else if (line.length() > 0 || !isspace(tmp)) {
            line += tmp;
        }
    }
}

void scan_i2c_bus()
{
    byte error, address;
    int nDevices;

    Serial.println("Scanning...");

    nDevices = 0;
    for (address = 1; address <= 0x7f; address++) {
        // The i2c_scanner uses the return value of
        // the Write.endTransmisstion to see if
        // a device did acknowledge to the address.
        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0) {
            Serial.print("I2C device found at address 0x");
            if (address < 16)
                Serial.print("0");
            Serial.print(address, HEX);
            Serial.println("  !");

            nDevices++;
        } else if (error == 4) {
            Serial.print("Unknown error at address 0x");
            if (address < 16)
                Serial.print("0");
            Serial.println(address, HEX);
        }
    }
    if (nDevices == 0)
        Serial.println("No I2C devices found");
    else
        Serial.println("done");
}

uint8_t *parse_data(const char *data, size_t hex_length, size_t &length)
{
    length = 0;
    size_t buffer_size = hex_length / 2 + 2;
    auto buffer = (uint8_t *)malloc(buffer_size);
    auto ptr = buffer;
#define DEBUG_PARSE_DATA 0
#if DEBUG_PARSE_DATA
    Serial.print("parse ");
    Serial.print(hex_length);
    Serial.print(' ');
#endif
    if (buffer) {
        int pos = 0;
        while(hex_length--) {
            pos++;
            if (*data == ',') {
                pos = 0;
            }
            if (pos == 2) {
                char buf[3] = {*(data - 1), *(data), 0};
                *ptr++ = strtoul(buf, nullptr, 16);
                length++;
                if (length >= buffer_size) {
                    break;
                }
                pos = 0;
#if DEBUG_PARSE_DATA
                Serial.print("0x");
                Serial.print((int)*(ptr - 1), 16);
                if (hex_length) {
                    Serial.print(',');
                }
#endif
            }
            data++;
        }
    }
#if DEBUG_PARSE_DATA
    Serial.println();
#endif
    return buffer;
}
