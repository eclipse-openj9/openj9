/*******************************************************************************
 * Copyright IBM Corp. and others 2024
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#if !defined(BUFFERWRITER_HPP_)
#define BUFFERWRITER_HPP_

#include "j9cfg.h"
#include "j9.h"

class VM_BufferWriter {
	/*
	 * Data members
	 */
	private:
	U_8 *_buffer;
	U_8 *_cursor;
	UDATA _size;
	U_8 *_bufferEnd;
	U_8 *_maxCursor;
	bool _overflow;

#if defined(J9VM_ENV_LITTLE_ENDIAN)
	static const bool _isLE = true;
#else
	static const bool _isLE = false;
#endif

	protected:

	public:

	/*
	 * Function members
	 */
	private:
	static VMINLINE U_16
	byteSwap(U_16 val)
	{
		U_16 swapped = ((val & 0xFF00) >> 8);
		swapped |= ((val & 0x00FF) << 8);

		return swapped;
	}

	static VMINLINE U_32
	byteSwap(U_32 val)
	{
		U_32 swapped = ((val & 0xFF000000) >> 24);
		swapped |= ((val & 0x00FF0000) >> 8);
		swapped |= ((val & 0x0000FF00) << 8);
		swapped |= ((val & 0x000000FF) << 24);

		return swapped;
	}

	static VMINLINE U_64
	byteSwap(U_64 val)
	{
		U_64 swapped = ((val & 0xFF00000000000000) >> 56);
		swapped |= ((val & 0x00FF000000000000) >> 40);
		swapped |= ((val & 0x0000FF0000000000) >> 24);
		swapped |= ((val & 0x000000FF00000000) >> 8);
		swapped |= ((val & 0x00000000FF000000) << 8);
		swapped |= ((val & 0x0000000000FF0000) << 24);
		swapped |= ((val & 0x000000000000FF00) << 40);
		swapped |= ((val & 0x00000000000000FF) << 56);

		return swapped;
	}

	protected:

	public:

	VM_BufferWriter(U_8 *buffer, UDATA size)
		: _buffer(buffer)
		, _cursor(buffer)
		, _size(size)
		, _bufferEnd(buffer + size)
		, _maxCursor(NULL)
		, _overflow(false)
	{
	}

	bool
	checkBounds(UDATA size)
	{
		if ((_cursor + size) >= _bufferEnd) {
			_overflow = true;
		}

		return !_overflow;
	}

	bool
	overflowOccurred()
	{
		return _overflow;
	}

	U_64
	getFileOffset(U_8 *bufferOffset, U_8 *from)
	{
		return (U_64)((UDATA)bufferOffset - (UDATA)from);
	}

	U_64
	getFileOffsetFromStart(U_8 *bufferOffset)
	{
		return getFileOffset(bufferOffset, _buffer);
	}

	U_8 *
	getBufferStart()
	{
		return _buffer;
	}

	UDATA
	getSize()
	{
		return getMaxCursor() - _buffer;
	}

	void
	writeU8NoCheck(U_8 val)
	{
		*_cursor = val;
		_cursor += sizeof(U_8);
	}

	void
	writeU8(U_8 val)
	{
		if (checkBounds(sizeof(U_8))) {
			writeU8NoCheck(val);
		}
	}

	void
	writeU16(U_16 val)
	{
		if (checkBounds(sizeof(U_16))) {
			U_16 newVal = val;
			if (_isLE) {
				newVal = byteSwap(val);
			}
			*(U_16 *)_cursor = newVal;
			_cursor += sizeof(U_16);
		}
	}

	void
	writeU32(U_32 val)
	{
		if (checkBounds(sizeof(U_32))) {
			U_32 newVal = val;
			if (_isLE) {
				newVal = byteSwap(val);
			}
			*(U_32 *)_cursor = newVal;
			_cursor += sizeof(U_32);
		}
	}

	void
	writeU64(U_64 val)
	{
		if (checkBounds(sizeof(U_64))) {
			U_64 newVal = val;
			if (_isLE) {
				newVal = byteSwap(val);
			}
			*(U_64 *)_cursor = newVal;
			_cursor += sizeof(U_64);
		}
	}

	void
	writeData(U_8 *data, UDATA size)
	{
		if (checkBounds(size)) {
			memcpy(_cursor, data, size);
			_cursor += size;
		}
	}

	U_8 *
	getAndIncCursor(UDATA size)
	{
		U_8 *old = _cursor;
		_cursor += size;

		return old;
	}

	U_8 *
	getCursor()
	{
		return _cursor;
	}

	U_8 *
	getMaxCursor()
	{
		if ((UDATA)_cursor > (UDATA)_maxCursor) {
			_maxCursor = _cursor;
		}
		return _maxCursor;
	}

	void
	setCursor(U_8 *cursor)
	{
		getMaxCursor();
		_cursor = cursor;
	}

	void
	writeLEB128(U_64 val)
	{
		if (checkBounds(9)) {
			U_64 newVal = val;

			do {
				U_8 byte = newVal & 0x7F;
				newVal >>= 7;

				if (newVal > 0) {
					byte |= 0x80;
				}
				writeU8NoCheck(byte);
			} while (newVal > 0);
		}
	}

	void
	writeLEB128PaddedU72(U_8 *cursor, U_64 val)
	{
		U_8 *old = _cursor;
		_cursor = cursor;
		writeLEB128PaddedU72(val);
		_cursor = old;
	}

	void
	writeLEB128PaddedU72(U_64 val)
	{
		if (checkBounds(9)) {
			U_64 newVal = val;

			writeU8NoCheck((newVal & 0x7F) | 0x80);
			writeU8NoCheck(((newVal >> 7) & 0x7F) | 0x80);
			writeU8NoCheck(((newVal >> 14) & 0x7F) | 0x80);
			writeU8NoCheck(((newVal >> 21) & 0x7F) | 0x80);
			writeU8NoCheck(((newVal >> 28) & 0x7F) | 0x80);
			writeU8NoCheck(((newVal >> 35) & 0x7F) | 0x80);
			writeU8NoCheck(((newVal >> 42) & 0x7F) | 0x80);
			writeU8NoCheck(((newVal >> 49) & 0x7F) | 0x80);
			writeU8NoCheck(((newVal >> 56) & 0x7F));
		}
	}

	void
	writeLEB128PaddedU64(U_8 *cursor, U_64 val)
	{
		U_8 *old = _cursor;
		_cursor = cursor;
		writeLEB128PaddedU64(val);
		_cursor = old;
	}

	void
	writeLEB128PaddedU64(U_64 val)
	{
		if (checkBounds(sizeof(U_64))) {
			U_64 newVal = val;

			writeU8NoCheck((newVal & 0x7F) | 0x80);
			writeU8NoCheck(((newVal >> 7) & 0x7F) | 0x80);
			writeU8NoCheck(((newVal >> 14) & 0x7F) | 0x80);
			writeU8NoCheck(((newVal >> 21) & 0x7F) | 0x80);
			writeU8NoCheck(((newVal >> 28) & 0x7F) | 0x80);
			writeU8NoCheck(((newVal >> 35) & 0x7F) | 0x80);
			writeU8NoCheck(((newVal >> 42) & 0x7F) | 0x80);
			writeU8NoCheck(((newVal >> 49) & 0x7F));
		}
	}

	void
	writeLEB128PaddedU32(U_8 *cursor, U_32 val)
	{
		U_8 *old = _cursor;
		_cursor = cursor;
		writeLEB128PaddedU32(val);
		_cursor = old;
	}

	void
	writeLEB128PaddedU32(U_32 val)
	{
		if (checkBounds(sizeof(U_32))) {
			U_64 newVal = val;

			writeU8NoCheck((newVal & 0x7F) | 0x80);
			writeU8NoCheck(((newVal >> 7) & 0x7F) | 0x80);
			writeU8NoCheck(((newVal >> 14) & 0x7F) | 0x80);
			writeU8NoCheck(((newVal >> 21) & 0x7F));
		}
	}

	void
	writeFloat(float val)
	{
		U_32 newVal = *(U_32 *)&val;
		writeU32(newVal);
	}

	static U_32
	convertFromLEB128ToU32(U_8 *start)
	{
		U_32 val = *start & 0x7F;

		if (J9_ARE_ALL_BITS_SET(*start, 0x80)) {
			start++;
			val |= (*start & 0X7F) << 7;
		}

		if (J9_ARE_ALL_BITS_SET(*start, 0x80)) {
			start++;
			val |= (*start & 0X7F) << 14;
		}

		if (J9_ARE_ALL_BITS_SET(*start, 0x80)) {
			start++;
			val |= (*start & 0X7F) << 21;
		}

		return val;
	}

	static U_64
	convertFromLEB128ToU64(U_8 *start)
	{
		U_64 val = *start & 0x7F;

		if (J9_ARE_ALL_BITS_SET(*start, 0x80)) {
			start++;
			val |= (U_64)(*start & 0X7F) << 7;
		}

		if (J9_ARE_ALL_BITS_SET(*start, 0x80)) {
			start++;
			val |= (U_64)(*start & 0X7F) << 14;
		}

		if (J9_ARE_ALL_BITS_SET(*start, 0x80)) {
			start++;
			val |= (U_64)(*start & 0X7F) << 21;
		}

		if (J9_ARE_ALL_BITS_SET(*start, 0x80)) {
			start++;
			val |= (U_64)(*start & 0X7F) << 28;
		}

		if (J9_ARE_ALL_BITS_SET(*start, 0x80)) {
			start++;
			val |= (U_64)(*start & 0X7F) << 35;
		}

		if (J9_ARE_ALL_BITS_SET(*start, 0x80)) {
			start++;
			val |= (U_64)(*start & 0X7F) << 42;
		}

		if (J9_ARE_ALL_BITS_SET(*start, 0x80)) {
			start++;
			val |= (U_64)(*start & 0X7F) << 49;
		}

		if (J9_ARE_ALL_BITS_SET(*start, 0x80)) {
			start++;
			val |= (U_64)(*start & 0X7F) << 56;
		}

		return val;
	}

};

#endif /* BUFFERWRITER_HPP_ */
