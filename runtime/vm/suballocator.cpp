/*******************************************************************************
 * Copyright IBM Corp. and others 2026
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdio>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include "suballocator.hpp"

Suballocator *Suballocator::create(J9PortLibrary *portLibrary, UDATA nSubspaces)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	Suballocator *instance = (Suballocator *)j9mem_allocate_memory(sizeof(Suballocator), J9MEM_CATEGORY_CLASSES);
	if (NULL != instance) {
		new (instance) Suballocator(portLibrary, nSubspaces);
	}

	return instance;
}

Suballocator::Suballocator(J9PortLibrary *portLibrary, UDATA n) :
	_nSubspaces(n),
	_memBase(NULL),
	_memBytes(0),
	_portLibrary(portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	_subspaces = (Subspace *)j9mem_allocate_memory(sizeof(Subspace) * _nSubspaces, J9MEM_CATEGORY_CLASSES);
}

Suballocator::~Suballocator()
{
	PORT_ACCESS_FROM_PORT(_portLibrary);
	for (UDATA i = 0; i < _nSubspaces; i++) {
		destroy_mspace(_subspaces[i]._ms);
	}
	j9mem_free_memory(_subspaces);
}

bool 
Suballocator::initializeSubs(bool restoreRun, void *base, UDATA capacity)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);

	_memBase = base;
	_memBytes = capacity;

	if (restoreRun) {
		return true;
	}

	UDATA sz = _memBytes/_nSubspaces;

	char *subspace = (char *)_memBase;
	for (UDATA i = 0; i < _nSubspaces; i++) {
		mspace ms = create_mspace_with_base(subspace, sz, 0);
		if (NULL == ms) {
			j9tty_printf(PORTLIB, "Error: failed to create mspace: addr=%p, sz=%lu\n", subspace, sz);
			return false;
		}
		mspace_set_footprint_limit(ms, sz);
		_subspaces[i]._ms = ms;
		_subspaces[i]._start = subspace;
		_subspaces[i]._size = sz;
		_subspaces[i]._end = subspace + sz;

		subspace += sz;
	}
	return true;
}

UDATA 
Suballocator::serializeSubs(IDATA fd)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);

	UDATA size = sizeof(Subspace) * _nSubspaces;
	UDATA written = write(fd, _subspaces, size);
	if (written != size) {
		j9tty_printf(PORTLIB, "write error =%d\n", errno);
		return 0;
	}
  	return size;
}

UDATA
Suballocator::deserializeSubs(IDATA fd)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);

	UDATA size = sizeof(Subspace) * _nSubspaces;
	UDATA readBytes = read(fd, _subspaces, size);
	if (readBytes != size) {
		j9tty_printf(PORTLIB, "read error=%d\n", errno);
		return 0;
	}
	return size;
}
UDATA 
Suballocator::getSerializedSize()
{
	return sizeof(Subspace) * _nSubspaces;
}
void * 
Suballocator::alloc(UDATA bytes)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);

	UDATA id = gettid();
	UDATA index = id % _nSubspaces;
	Subspace msp = _subspaces[index];

	void *mem = mspace_malloc(msp._ms, bytes);
	if (mem < msp._start || mem >= msp._end) {
		j9tty_printf(PORTLIB, "Alloc Error: thread %d allocate memory in subspace %d out of bounds\n", id, index);
		return NULL;
	}
	return mem;
}

void
Suballocator::dealloc(void *p)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);

	UDATA low = 0;
	UDATA high = _nSubspaces-1;
	while (high >= 0 && low <= (_nSubspaces-1)) {
		int mid = low + (high - low) / 2;

		Subspace &msp = _subspaces[mid];
		if (p >= msp._start && p < msp._end) {
 			mspace_free(msp._ms, p);
			return;
		} else if (p >= msp._end) {
			low = mid + 1;
		} else {
			high = mid - 1;
		}
	}
	j9tty_printf(PORTLIB, "Dealloc Error: pointer %p not allocated by suballocator\n", p);
}

void * 
Suballocator::realloc(void *p, UDATA bytes)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);

	for (UDATA i = 0; i < _nSubspaces; i++) {
		Subspace msp = _subspaces[i];
		mspace ms = msp._ms;

		if (p >= msp._start && p < msp._end) {
			void *mem = mspace_realloc(ms, p, bytes);
			if (mem < msp._start || mem >= msp._end) {
				j9tty_printf(PORTLIB, "Realloc Error: pointer %p not in subspace %d bounds\n", mem, i);
				return NULL;
			}
			return mem;
		}
	}

	j9tty_printf(PORTLIB, "Dealloc Error: pointer %p not allocated by suballocator\n", p);
	return NULL;
}

void 
Suballocator::dumpStats()
{
	PORT_ACCESS_FROM_PORT(_portLibrary);

	for (UDATA i = 0; i < _nSubspaces; i++) {
		Subspace msp = _subspaces[i];
		j9tty_printf(PORTLIB, "Subspace %d: start=%p, end=%p, size=%zu\n", i, msp._start, msp._end, msp._size);
	}
}
