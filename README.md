# I2C UART Bridge

This class can be used as drop in replacement for the TwoWire class, using the I2C protocol over UART.

It supports master-slave and master-master communication.


## Example

// +i2ct=17,89,22
Wire.beginTransmission(0x17);
Wire.write(0x89);
Wire.write(0x22);
if (Wire.endTransmission() == 0) {

	// +i2cr=17,02
	uint16_t word;
	if (Wire.requestFrom(0x17, 2) == 2) {
		Wire.readBytes(reinterpret_cast<const uint8_t *>(&word), sizeof(word));
	}
}
