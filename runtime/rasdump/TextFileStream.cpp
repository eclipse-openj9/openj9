/*******************************************************************************
 * Copyright (c) 2003, 2019 IBM Corp. and others
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
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

/* Includes */
#include <string.h>
#include "TextFileStream.hpp"
#include "j9.h"
#include "j9port.h"
#include "rasdump_internal.h"

/* Constructor */
TextFileStream::TextFileStream(J9PortLibrary* portLibrary) :
	_Buffer(NULL),
	_IsOpen(false),
	_BufferPos(0),
	_BufferSize(16*1024),
	_PortLibrary(portLibrary),
	_FileHandle(-1),
	_Error(false)
{
	_Buffer = (char *) OMRPORT_FROM_J9PORT(_PortLibrary)->mem_allocate_memory(OMRPORT_FROM_J9PORT(_PortLibrary), _BufferSize, "TextFileStream::TextFileStream", OMRMEM_CATEGORY_VM);
	if (_Buffer == NULL) {
		_BufferSize = 0;
	}
}

/* Destructor */
TextFileStream::~TextFileStream()
{
	close();
}

/* Method for opening the file */
void
TextFileStream::open(const char* fileName, bool cacheWrites)
{
	PORT_ACCESS_FROM_PORT(_PortLibrary);
	if (0 == strcmp(fileName, J9RAS_STDOUT_NAME)) {
		_FileHandle = J9PORT_TTY_OUT; 
	} else if (0 == strcmp(fileName, J9RAS_STDERR_NAME)) {
		_FileHandle = J9PORT_TTY_ERR;
	} else {
		_FileHandle = j9file_open(fileName, EsOpenWrite | EsOpenCreate | EsOpenTruncate | EsOpenCreateNoTag, 0666);
		if (_FileHandle != -1) {
			_IsOpen = true;
		}
		
	}
	if(!cacheWrites) {
		_BufferSize = 0;
	}
}

/* Method for closing the file */
void 
TextFileStream::close(void)
{
	PORT_ACCESS_FROM_PORT(_PortLibrary);
	if (_FileHandle != -1) {
		if(_BufferSize != 0) {
			j9file_write_text(_FileHandle, _Buffer, _BufferPos);
		}
		j9file_sync(_FileHandle);
		if (_IsOpen) {
			/*If we don't open the file stream, it means that we don't close it either */
			j9file_close(_FileHandle);
		}
	}

	_FileHandle = -1;	
	_Error      = false;
	if(_Buffer) {
		j9mem_free_memory(_Buffer);
		_Buffer = NULL;
	}
}

/* Methods for getting the object's status */
bool TextFileStream::isOpen(void) const
{
	return _FileHandle != -1;
}

bool TextFileStream::isError(void) const
{
	return _Error;
}

/* Method for writing characters described by a pointer and a length to the file*/
void
TextFileStream::writeCharacters(const char* data, IDATA length)
{
	PORT_ACCESS_FROM_PORT(_PortLibrary);
	/* deal with the simple no-handle and non-cached cases */
	if(_FileHandle == -1) {
		return;
	}
	if(_BufferSize == 0) {
		_Error = _Error || j9file_write_text(_FileHandle, data, length);
		return;
	}

	/* we are now in a cached state.  3 possible cases
		1) the data fits in a buffer without flushing - just copy it in
		2) simple overflows - finish this buffer, write it out, then put remaining in the newly empty buffer
		3) big overflow - finish this buffer, write it out, then write out the remaining data all at once, finally leaving _Buffer empty
	*/
	UDATA spaceRemainingInBuffer = _BufferSize - _BufferPos;
	UDATA bytesToCopyIntoBuffer = OMR_MIN(spaceRemainingInBuffer, (UDATA) length);  // copy up to the end of the buffer
	UDATA remainingBytes = (UDATA) length - bytesToCopyIntoBuffer;

	memcpy(&_Buffer[_BufferPos], data, bytesToCopyIntoBuffer);
	_BufferPos += bytesToCopyIntoBuffer;

	if(_BufferPos == _BufferSize) {
		/* flush full & copy remaining */
		_BufferPos = 0;
		_Error = _Error || j9file_write_text(_FileHandle, _Buffer, _BufferSize);
		if(remainingBytes < _BufferSize) {
			memcpy(_Buffer, data+bytesToCopyIntoBuffer, remainingBytes);
			_BufferPos = remainingBytes;
		} else {
			/* what's left is fully bigger than a single buffer - just write it out */
			_Error = _Error || j9file_write_text(_FileHandle, data+bytesToCopyIntoBuffer, remainingBytes);
		}
	}
}

/* Method for writing characters described by a J9UTF8 structure to the file*/
void
TextFileStream::writeCharacters(const J9UTF8* data)
{
	writeCharacters((char*)data + offsetof(J9UTF8, data), J9UTF8_LENGTH(data));
}

/* Method for writing characters described by a pointer to the file*/
void
TextFileStream::writeCharacters(const char* data)
{
	writeCharacters(data, strlen(data));
}

/* Method for writing a pointer as characters to the file*/
void
TextFileStream::writePointer(const void *data, bool printPrefix)
{
	const char *format = printPrefix ? "0x%p" : "%p";
	
	writeInteger((IDATA)data, format);
}

/* Method for writing a number as characters to the file*/
void
TextFileStream::writeInteger(UDATA data, const char *format)
{
	char buffer[32];
	PORT_ACCESS_FROM_PORT(_PortLibrary);
	UDATA length;
	
	length = j9str_printf(PORTLIB, buffer, sizeof(buffer), format, data);

	writeCharacters(buffer, length);
}

/* Method for writing a number as characters to the file*/
void
TextFileStream::writeInteger64(U_64 data, const char *format)
{
	char buffer[32];
	PORT_ACCESS_FROM_PORT(_PortLibrary);
	UDATA length;
	
	length = j9str_printf(PORTLIB, buffer, sizeof(buffer), format, data);

	writeCharacters(buffer, length);
}

void
TextFileStream::writeVPrintf(const char *format, ...)
{
	/* Currently, this appears to be the max buffer size needed. We use
	 * this routine to print 128-bit values and, in this case, we need
	 * a buffer of at least size 33 in order to accommodate the '\0'. 
	 */
	char buffer[35];
	PORT_ACCESS_FROM_PORT(_PortLibrary);
	va_list args;
	UDATA length;

	va_start( args, format );
	length = j9str_vprintf(buffer, sizeof(buffer), format, args);
	va_end( args );

	writeCharacters(buffer, length);
}

/* Write bytes out with commas between every thousand. */
void
TextFileStream::writeIntegerWithCommas(U_64 value)
{
	U_16 thousandsStack[7]; /* log1000(2^64-1) = 6.42. We never need more than 7 frames. */
	U_8  stackTop = 0;
	U_64 workingValue = value;

	do {
		thousandsStack[stackTop] = (U_16)(workingValue % 1000);
		stackTop += 1;
		workingValue /= 1000;
	} while (workingValue != 0);

	const char * format = "%zu";

	do {
		stackTop -= 1;
		writeInteger(thousandsStack[stackTop], format);
		format = ",%03zu";
	} while (stackTop != 0);
}
