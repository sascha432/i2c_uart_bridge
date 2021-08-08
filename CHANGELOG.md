# Changelog

## 0.2.1-dev

## 0.2.0

- Code size and memory usage optimized
- Compiles with GCC 4.8.2, 5.4.0, 7.3, 10.2 and Visual Studio 2019
- StreamString dependency removed
- Added RESPONSE command to avoid slaves receiving own transmissions if the master echos everything to the serial port (+I2CA)
- Optimized code and reduced size by ~1300byte, minor increase in memory usage (~8byte)
- Support for the old protocol using +I2CT instead of +I2CA

## 0.1.0

- Initial version with no changelog
