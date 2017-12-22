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
#ifndef FILESTREAM_HPP
#define FILESTREAM_HPP

/* Includes */
#include "j9port.h"

/**************************************************************************************************/
/*                                                                                                */
/* Class for writing to a file                                                                    */
/*                                                                                                */
/**************************************************************************************************/
class FileStream
{
public :
	/* Constructor */
	FileStream(J9PortLibrary* portLibrary);

	/* Destructor */
	~FileStream();

	/* Method for opening the file */
	void open(const char* fileName);

	/* Method for closing the file */
	void close(void);

	/* Methods for getting the object's status */
	bool isOpen(void) const;
	bool hasError(void) const;

	/* Methods for writing data to the file */
	void writeCharacters (const char* data, IDATA length);
	void writeCharacters (const char* data);
	void writeNumber     (IDATA data, int length);

private :
	/* Prevent use of the copy constructor and assignment operator */
	FileStream(const FileStream& source);
	FileStream& operator=(const FileStream& source);

protected :
	/* Declared data */
	J9PortLibrary* _PortLibrary;
	IDATA          _FileHandle;
	IDATA          _Error;
};

#endif
