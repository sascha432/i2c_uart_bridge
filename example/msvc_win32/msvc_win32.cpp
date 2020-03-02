/**
  Author: sascha_lammers@gmx.de
*/

// TwoWire Mock class to communicate with I2C bus over USB and an Arduino

#include <Arduino_compat.h>
#include "LiquidCrystal_I2C.h"

int main()
{
    HANDLE hComm = get_com_handle();

    Serial.println(
        "---------------------------------------------------------------------------\n"
        " ping                           Send ping\n"
        " scan                           Scan I2C bus\n"
        " clear                          Clear display\n"
        " setbl=<1/0>                    Set backlight\n"
        " print=<x>,<y>,<text>           Display \"text\" at coordinates x/y\n"
        " println=<text>                 Display \"text\"\n"
        " lm75=<address=0x48>            Read LM75A at <address>\n"
        " exit                           End program\n"
        "---------------------------------------------------------------------------\n"
    );


    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD  fdwMode = ENABLE_WINDOW_INPUT;
    SetConsoleMode(hStdin, fdwMode);

    Serial.println(F("Waiting for device to reboot..."));
    EscapeCommFunction(hComm, SETDTR);
    delay(10);
    EscapeCommFunction(hComm, CLRDTR);

    LiquidCrystal_I2C lcd(0x27, 16, 2);

    String c_line;
    bool terminate = false;
    do
    {
        DWORD read;

        if (GetNumberOfConsoleInputEvents(hStdin, &read) && read) {
            INPUT_RECORD c_data;
            ReadConsoleInputA(hStdin, &c_data, 1, &read);
            switch (c_data.EventType)
            {
            case KEY_EVENT:
               if (c_data.Event.KeyEvent.bKeyDown == FALSE) {
                    if (c_data.Event.KeyEvent.uChar.AsciiChar == 0) {
                    }
                    else if (c_data.Event.KeyEvent.uChar.AsciiChar == '\r') {
                        Serial.println();
                        auto pos = c_line.indexOf('=');
                        String cmd;
                        if (pos != -1) {
                            cmd = c_line.substring(0, pos);
                            c_line.remove(0, pos + 1);
                        }
                        else {
                            cmd = c_line;
                        }
                        if (cmd == "scan") {
                            serial_write(hComm, "+I2CS\n");
                        }
                        else if (cmd == "exit") {
                            terminate = true;
                            Serial.println("Exiting...");
                        }
                        else if (cmd == "ping") {
                            serial_write(hComm, "+PING\n");
                        }
                        else if (cmd == "lm75") {
                            int address = (int)strtoul(c_line.c_str(), nullptr, 0);
                            if (address == 0) {
                                address = 0x48;
                            }
                            Wire.beginTransmission(address);
                            Wire.write(0);
                            if (Wire.endTransmission() == 0) {
                                if (Wire.requestFrom(address, 2) == 2) {
                                    float temp = ((Wire.read() << 8) | Wire.read()) / 256.0f;
                                    Serial.printf("Temperature %.2fC\n", temp);
                                }
                            }
                        }
                        else if (cmd == "setbl") {
                            lcd.setBacklight(c_line.toInt());
                        }
                        else if (cmd == "clear") {
                            lcd.clear();
                            lcd.home();
                        }
                        else if (cmd == "println") {
                            lcd.println(c_line);
                        }
                        else {
                            Serial.print("Unknown command");
                            Serial.println(cmd);
                        }
                        c_line = String();
                    }
                    else if (c_data.Event.KeyEvent.uChar.AsciiChar == '\n') {
                    }
                    else if (c_data.Event.KeyEvent.uChar.AsciiChar == '\b') {
                        Serial.print("\b \b");
                        if (c_line.length()) {
                            c_line.remove(c_line.length() - 1, 1);
                        }                        
                    }
                    else {
                        Serial.print(c_data.Event.KeyEvent.uChar.AsciiChar);
                        c_line += c_data.Event.KeyEvent.uChar.AsciiChar;
                    }
                }
                break;
            default:
                break;
            }
        }

        auto tmp = serial_read_byte(hComm);
        if (tmp == "+PING") {
            Serial.println("LCD init @ 0x27");
            lcd.init();
            //lcd.backlight();
            //lcd.clear();
            Serial.println("Waiting for user command...");
        }
        else if (tmp.length()) {
            Serial.println(tmp.c_str());
        }

        delay(10);
    }
    while (!terminate);

    CloseHandle(hComm);

    return 0;
}