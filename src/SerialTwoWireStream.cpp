/**
 * Author: sascha_lammers@gmx.de
 */

#include "SerialTwoWireStream.h"
#include "SerialTwoWireDebug.h"

#if SERIALTWOWIRE_USE_OWN_STREAM_CLASS

void SerialTwoWireStream::clear()
{
	_position = 0;
	_length = 0;
	resize(_get_block_size(0));
}

void SerialTwoWireStream::release()
{
	_size = _resize(0);
	_length = 0;
	_position = 0;
}

SerialTwoWireStream::size_type SerialTwoWireStream::write(uint8_t data)
{
	if (_length < _size) {
		_buffer[_length++] = data;
	}
	else {
		if (!resize(_length + 1)) {
			release();
			return 0;
		}
		_buffer[_length++] = data;
	}
	return 1;
}

SerialTwoWireStream::size_type SerialTwoWireStream::write(const uint8_t *data, size_t len)
{
	auto data_len = (size_type)len;
	size_type required = data_len + _length;
	if (required >= _size) {
		if (!resize(required + 1)) {
			return 0;
		}
	}
#if __AVR__
	memcpy(&_buffer[_length], data, data_len);
#else
	std::copy_n(data, data_len, &_buffer[_length]);
#endif
	_length += data_len;
	return data_len;
}

SerialTwoWireStream::size_type SerialTwoWireStream::read(uint8_t *data, size_t len)
{
	size_type avail = _length - _position;
	size_type data_len = len > avail ? avail : (size_type)len;
#if __AVR__
	memcpy(data, begin(), len);
#else
	std::copy_n(begin(), data_len, data);
#endif
	_position += data_len;
	return data_len;
}


bool SerialTwoWireStream::resize(size_type new_size)
{
	auto blockSize = _get_block_size(new_size);
	if (blockSize != _size) {
		_size = _resize(blockSize);
		if (_length > _size) {
			_length = _size;
		}
		if (_position > _length) {
			_position = _length;
		}
#if DEBUG_SERIALTWOWIRE
		std::fill(_buffer + _length, _buffer + _size, '#');
#endif
	}
	return true;
}

SerialTwoWireStream::size_type SerialTwoWireStream::_get_block_size(size_type new_size) const
{
	size_type blockSize = new_size < _allocMinSize ? _allocMinSize : new_size;
	if (kAllocBlockBitMask != 0) {
		return (blockSize + kAllocBlockBitMask) & ~kAllocBlockBitMask;
	}
	else {
		return blockSize + kAllocBlockSize - (blockSize % kAllocBlockSize);
	}
}

SerialTwoWireStream::size_type SerialTwoWireStream::_resize(size_type new_size)
{
	if (new_size != 0) {
		if (_buffer) {
			_buffer = (uint8_t *)realloc(_buffer, new_size);
			__LDBG_assert_printf(!!_buffer, "size=%u old_size=%u", new_size, _size);
		}
		else {
			_buffer = (uint8_t *)malloc(new_size);
			__LDBG_assert_printf(!!_buffer, "size=%u", new_size);
		}
		if (_buffer) {
			return new_size;
		}
	}
	else if (_buffer) {
		free(_buffer);
		_buffer = nullptr;
	}
	return 0;
}

#endif
