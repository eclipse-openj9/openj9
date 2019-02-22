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
#ifndef TEXTFILESTREAM_HPP
#define TEXTFILESTREAM_HPP

/* Includes */
#include "j9port.h"

/* Declarations to avoid inclusions */
struct J9UTF8;

/**************************************************************************************************/
/*                                                                                                */
/* Class for writing to a text file                                                                    */
/*                                                                                                */
/**************************************************************************************************/
class TextFileStream
{
public :
	/* Constructor */
	TextFileStream(J9PortLibrary* portLibrary);

	/* Destructor */
	~TextFileStream();

	/* Method for opening the file */
	void open(const char* fileName, bool cacheWrites);

	/* Method for closing the file */
	void close(void);

	/* Methods for getting the object's status */
	bool isOpen(void) const;
	bool isError(void) const;

	/* Methods for writing data as characters to the file */
	void writeCharacters (const char*   data, IDATA length);
	void writeCharacters (const J9UTF8* data);
	void writeCharacters (const char*   data);
	void writePointer    (const void *data, bool printPrefix=true);
	void writeInteger    (UDATA data, const char *format="0x%zX");
	void writeInteger64  (U_64 data, const char *format="0x%llX");
	void writeIntegerWithCommas(U_64 value);
	void writeVPrintf    (const char *format, ...);

private :
	/* Prevent use of the copy constructor and assignment operator */
	TextFileStream(const TextFileStream& source);
	TextFileStream& operator=(const TextFileStream& source);
	char *_Buffer;
	bool _IsOpen;
	UDATA _BufferPos;
	UDATA _BufferSize;

protected :
	/* Declared data */
	J9PortLibrary* _PortLibrary;
	IDATA          _FileHandle;
	bool           _Error;
};

#endif /* TEXTFILESTREAM_HPP */
