/**
 StreamString.cpp

 Copyright (c) 2015 Markus Sattler. All rights reserved.
 This file is part of the esp8266 core for Arduino environment.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

 */

#include <Arduino.h>
#include "StreamString.h"

size_t StreamString::write(const uint8_t *data, size_t size) {
	if (!data || !size || !reserve(size)) {
		invalidate();
		return 0;
	}
	len = size;
	memcpy(buffer, (const char *)data, size);
    buffer[size] = 0;
    return size;
}

size_t StreamString::write(uint8_t data) {
    return concat((char) data);
}

void StreamString::remove(unsigned int index, unsigned int count) {
    if (index >= len) { return; }
    if (count <= 0) { return; }
    if (count > len - index) { count = len - index; }
    char *writeTo = buffer + index;
    len = len - count;
    memcpy(writeTo, buffer + index + count,len - index);
    buffer[len] = 0;
}

int StreamString::available() {
    return length();
}

int StreamString::read() {
    if(length()) {
        char c = buffer[0];
        remove(0, 1);
        return c;

    }
    return -1;
}

int StreamString::peek() {
    if(length()) {
        return buffer[0];
    }
    return -1;
}

void StreamString::flush() {
}
