/*******************************************************************************
 * Copyright (c) 2003, 2014 IBM Corp. and others
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
#include "FileStream.hpp"
#include "../oti/util_api.h"

/* Constructor */
FileStream::FileStream(J9PortLibrary* portLibrary) :
	_PortLibrary(portLibrary),
	_FileHandle(-1),
	_Error(0)
{
	/* Nothing to do */
}

/* Destructor */
FileStream::~FileStream()
{
	close();
}

/* Method for opening the file */
void
FileStream::open(const char* fileName)
{
	if (fileName[0] != '-' ) {
		_FileHandle = j9cached_file_open(_PortLibrary, fileName, EsOpenWrite | EsOpenCreate | EsOpenTruncate | EsOpenCreateNoTag, 0666);
		_Error = 0;
	}
}

/* Method for closing the file */
void 
FileStream::close(void)
{
	if (_FileHandle != -1) {
		j9cached_file_sync(_PortLibrary, _FileHandle);
		j9cached_file_close(_PortLibrary, _FileHandle);
	}

	_FileHandle = -1;	
}

/* Methods for getting the object's status */
bool FileStream::isOpen(void) const
{
	return _FileHandle != -1;
}

bool FileStream::hasError(void) const
{
	return _Error != 0;
}

/* Method for writing characters described by a pointer and a length to the file*/
void
FileStream::writeCharacters(const char* data, IDATA length)
{
	if (_FileHandle != -1 && ! _Error) {
		IDATA rc = j9cached_file_write(_PortLibrary, _FileHandle, data, length);

		if (rc != length) {
			_Error = rc;
		}
	}
}

void
FileStream::writeCharacters(const char* data)
{
	writeCharacters(data, strlen(data));
}

/* Method for writing a number to the file */
void
FileStream::writeNumber(IDATA data, int length)
{
	/* Validate the parameters */
	IDATA number = data;
	int   count  = (length > 8) ? 8 : length;

	/* Copy the characters of the number to a buffer in network order encoding */
	char buffer[8] = {0,0,0,0,0,0,0,0};

	while (count-- > 0) {
		buffer[count] = (char)(number & 0xFF);
		number >>= 8;
	}

	/* Write the data to the file */
	writeCharacters(buffer, length);
}
