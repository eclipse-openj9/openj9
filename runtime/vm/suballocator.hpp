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

#ifndef SUBALLOCATOR_HPP
#define SUBALLOCATOR_HPP

#include "dlmalloc.h"
#include "j9.h"
#include "j9port_generated.h"
#include "j9protos.h"

class Suballocator 
{
private:
	struct Subspace 
	{
		mspace _ms;
		void *_start;
		void *_end;
		UDATA _size;
	};

	UDATA _nSubspaces;
	Subspace *_subspaces;
	void *_memBase;
	UDATA _memBytes;
	J9PortLibrary *_portLibrary;

	Suballocator(J9PortLibrary *portLibrary, UDATA n);

protected:
	void *operator new(size_t size, void *memoryPointer) { return memoryPointer; }

public:
	static Suballocator *create(J9PortLibrary *portLibrary, UDATA nSubspaces);

	bool initializeSubs(bool restoreRun, void *base, UDATA capacity);
	UDATA serializeSubs(IDATA fd);
	UDATA deserializeSubs(IDATA fd);
	UDATA getSerializedSize();
	void* getMemSpace() { return _memBase; }
	UDATA getMemSize() { return _memBytes; }
	void dumpStats();

	~Suballocator();

	void *alloc(UDATA bytes);
	void dealloc(void *p);
	void *realloc(void *p, UDATA bytes);
};
#endif /* SUBALLOCATOR_HPP */
