/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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

/*
 * BufferManager.hpp
 */

#ifndef BUFFERMANAGER_HPP_
#define BUFFERMANAGER_HPP_

/* @ddr_namespace: default */
#include "j9comp.h"

struct J9PortLibrary;

class BufferManager
{
public:

	BufferManager(J9PortLibrary *portLibrary, UDATA bufferSize, U_8 **buffer);
	~BufferManager();

	bool isOK() { return NULL != *_buffer; }

	void *alloc(UDATA size);
	void reclaim(void *memory, UDATA actualSize);
	void free(void *memory)	{
		/*
		 * Do nothing, we will free the entire buffer when the BufferManager itself is free'd.
		 */
	}

private:
	J9PortLibrary *_portLibrary;
	UDATA _bufferSize;
	U_8 **_buffer;
	UDATA _pos;
	void *_lastAllocation;
	bool _shouldFreeBuffer;
};

#endif /* BUFFERMANAGER_HPP_ */
