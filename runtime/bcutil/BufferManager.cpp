/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
 * BufferManager.cpp
 *
 */
#include "j9comp.h"
#include "j9.h"
#include "ut_j9bcu.h"

#include "BufferManager.hpp"

BufferManager::BufferManager(J9PortLibrary *portLibrary, UDATA bufferSize, U_8 **buffer) :
	_portLibrary(portLibrary),
	_bufferSize(bufferSize),
	_buffer(buffer),
	_pos(0),
	_shouldFreeBuffer(false)
{
	if ( NULL == *_buffer ) {
		PORT_ACCESS_FROM_PORT(_portLibrary);
		U_8 *ptr = (U_8*)j9mem_allocate_memory(_bufferSize, J9MEM_CATEGORY_CLASSES);
		if ( NULL == ptr ) {
			/*
			 * Not enough native memory to complete this ROMClass load.
			 */
			_bufferSize = 0;
		} else {
			/*
			 * Pass back the newly allocated buffer to the caller.
			 */
			*_buffer = ptr;
		}
	}
}

BufferManager::~BufferManager()
{
	if ( _shouldFreeBuffer ) {
		PORT_ACCESS_FROM_PORT(_portLibrary);
		j9mem_free_memory(*_buffer);
		*_buffer = NULL;
	}
}

void *
BufferManager::alloc(UDATA size)
{
	U_8 *memory = NULL;
	if ((_pos + size) <= _bufferSize) {
		memory = *_buffer + _pos;
		_lastAllocation = memory;
		_pos += size;
	} else {
		/*
		 * If an allocation failed, free the original buffer.
		 */
		_shouldFreeBuffer = true;
	}
	return memory;
}

void
BufferManager::reclaim(void *memory, UDATA actualSize)
{
	if (memory == _lastAllocation) {
		UDATA newPos = UDATA(_lastAllocation) - UDATA(*_buffer) + actualSize;
		if (newPos <= _pos) {
			_pos = newPos;
			return;
		}
	}

	Trc_BCU_Assert_ShouldNeverHappen();
}
