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

bool SerialTwoWireStream::resize(size_type new_size)
{
	auto blockSize = _get_block_size(new_size);
	if (blockSize != _size) {
		if (kAllocMinSize <= kAllocBlockSize && blockSize == 0) {
			release();
			return true;
		}
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
	size_type blockSize = new_size < kAllocMinSize ? kAllocMinSize : new_size;
	if (kAlllocBlockBitMask != 0) {
		return (blockSize + kAlllocBlockBitMask) & ~kAlllocBlockBitMask;
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
		__LDBG_free(_buffer);
	}
	return 0;
}

#endif