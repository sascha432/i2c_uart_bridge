/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "SerialTwoWireStream.h"

#pragma push_macro("new")
#undef new

inline SerialTwoWireStream::SerialTwoWireStream() :
	_buffer(nullptr),
	_length(0), _size(0),
	_position(0),
	_allocMinSize(kAllocMinSize)
{
}

inline SerialTwoWireStream::~SerialTwoWireStream() 
{
    release();
}

//inline SerialTwoWireStream::SerialTwoWireStream(SerialTwoWireStream &&move) noexcept :
//    _buffer(std::exchange(move._buffer, nullptr)),
//    _length(std::exchange(move._length, 0)),
//    _size(std::exchange(move._size, 0)),
//    _position(std::exchange(move._position, 0)),
//    _allocMinSize(move._allocMinSize)
//{
//}
//
//inline SerialTwoWireStream &SerialTwoWireStream::operator=(SerialTwoWireStream &&move) noexcept
//{
//    this->~SerialTwoWireStream();
//    new (this) SerialTwoWireStream(std::move(move));
//    return*this;
//}

inline void SerialTwoWireStream::setAllocMinSize(uint8_t size) 
{
    _allocMinSize = size;
}

inline int SerialTwoWireStream::read() 
{
    if (_position < _length) {
        return _buffer[_position++];
    }
    return -1;
}

inline int SerialTwoWireStream::peek() 
{
    if (_position < _length) {
        return _buffer[_position];
    }
    return -1;
}

inline uint8_t SerialTwoWireStream::charAt(int offset) const 
{
    if ((size_t)offset >= _length) {
        return 0;
    }
    return _buffer[offset];
}

inline uint8_t SerialTwoWireStream::operator[](int offset) const 
{
    return _buffer[offset];
}

inline uint8_t &SerialTwoWireStream::operator[](int offset) 
{
    return _buffer[offset];
}

inline SerialTwoWireStream::size_type SerialTwoWireStream::length() const 
{
	return _length;
}

inline size_t SerialTwoWireStream::available() const 
{
	return _length - _position;
}

inline SerialTwoWireStream::size_type SerialTwoWireStream::size() const 
{
	return _size;
}

inline bool SerialTwoWireStream::empty() const 
{
	return _length == _position;
}

inline SerialTwoWireStream::size_type SerialTwoWireStream::push_back(uint8_t data) 
{
	return write(data);
}

inline int SerialTwoWireStream::pop_back() 
{
	if (_length > 0) {
		if (_position >= _length) {
			_position--;
		}
		return _buffer[--_length];
	}
	return -1;
}

inline int SerialTwoWireStream::pop_front() 
{
	return read();
}

inline uint8_t *SerialTwoWireStream::begin() 
{
	return &_buffer[_position];
}

inline uint8_t *SerialTwoWireStream::end() 
{
	return &_buffer[_length];
}

inline size_t SerialTwoWireStream::readBytes(uint8_t *data, size_t len) 
{
    return read(data, len);
}

inline bool SerialTwoWireStream::reserve(size_type new_size) 
{
    return new_size <= _size ? true : resize(new_size);
}

#pragma pop_macro("new")
