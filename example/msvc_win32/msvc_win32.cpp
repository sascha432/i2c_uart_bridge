/**
  Author: sascha_lammers@gmx.de
*/

// TwoWire Mock class to communicate with I2C bus over USB and an Arduino

// LiquidCrystal_I2C does not work yet, probably timing issues...

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1


#include <Arduino_compat.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
//#include "LiquidCrystal_I2C.h"
//#include <hd44780.h>
//#include <hd44780ioClass/hd44780_I2Cexp.h>


int main()
{
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
    CommSerial.setDTR();
    delay(10);
    CommSerial.clearDTR();

    //hd44780_I2Cexp lcd(0x27, I2Cexp_PCF8574, 0, 1, 2, 4, 5, 6, 7, 3, HIGH);
    //LiquidCrystal_I2C lcd(0x27, 16, 2);

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
                            CommSerial.println("+I2CS");
                        }
                        else if (cmd == "exit") {
                            terminate = true;
                            Serial.println("Exiting...");
                        }
                        else if (cmd == "ping") {
                            CommSerial.println("+PING");
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
                            //lcd.setBacklight((uint8_t)c_line.toInt());
                        }
                        else if (cmd == "clear") {
                            //lcd.clear();
                            //lcd.home();
                        }
                        else if (cmd == "println") {
                            //lcd.print(c_line);
                        }
                        else {
                            Serial.print("Unknown command: ");
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

        if (CommSerial.available()) {
            auto tmp = CommSerial.readStringUntil('\n');
            tmp.trim();
            if (tmp == "+PING") {
//                Serial.println("LCD init @ 0x27");
                //lcd.init();
                //lcd.backlight();
                //lcd.clear();

                Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

                if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
                    Serial.println(F("SSD1306 allocation failed"));
                }
                display.display();
                
                delay(3000);

                //auto status = lcd.begin(16, 2);
                //if (status)
                //{
                //    status = -status;
                //    __debugbreak_and_panic_printf_P(PSTR("fatal error %d\n"), status);
                //}
                //lcd.print("Hello, World!");

                Serial.println("Waiting for user command...");
            }
            else if (tmp.length()) {
                Serial.println(tmp.c_str());
            }
        }

        delay(10);
    }
    while (!terminate);

    

    return 0;
}