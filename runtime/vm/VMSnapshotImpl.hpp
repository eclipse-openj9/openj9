/*******************************************************************************
 * Copyright (c) 2024 IBM Corp. and others
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
* VMSnapshotImpl.hpp
*/

#ifndef VMSNAPSHOTIMPL_HPP_
#define VMSNAPSHOTIMPL_HPP_

#include "j9.h"
#include "j9comp.h"
#include "j9port_generated.h"
#include "j9protos.h"
#include "objhelp.h"
#include "ut_j9vm.h"
#include "vm_api.h"

class VMSnapshotImpl
{
private:
	J9JavaVM *_vm;
	J9PortLibrary *_portLibrary;
	J9SnapshotHeader *_snapshotHeader;
	J9MemoryRegion *_memoryRegions;
	J9Heap *_heap;
	J9Heap *_heap32;
	J9ITable *_invalidITable;
	const char *_ramCache;
	omrthread_monitor_t _vmSnapshotImplMonitor;
	VMSnapshotImplPortLibrary *_vmSnapshotImplPortLibrary;

public:
	/* TODO: reallocation will fail so initial heap size is large (Should be PAGE_SIZE aligned) */
	/* TODO: initial image size restriction will be removed once MMAP MAP_FIXED removed. see @ref VMSnapshotImpl::readImageFromFile */
	static const UDATA GENERAL_MEMORY_SECTION_SIZE = 100 * 1024 * 1024;
	static const UDATA SUB4G_MEMORY_SECTION_SIZE = 100 * 1024 * 1024;
	static const UDATA CLASS_LOADER_REMOVE_COUNT = 8;

private:
	bool initializeMonitor(void);
	bool initializeInvalidITable(void);

	/* Image Heap initialization and allocation functions */
	void* allocateImageMemory();
	void* reallocateImageMemory(UDATA size); /* TODO: Function will be used once random memory loading is allowed. Function not used currently */
	void* initializeHeap(void);

	bool allocateImageTableHeaders(void);

	bool readImageFromFile(void);
	bool writeImageToFile(void);

	/* Fixup functions called during teardown sequence */
	void fixupClassLoaders(void);
	void fixupClasses(void);
	void fixupClass(J9Class *clazz);
	void fixupArrayClass(J9ArrayClass *clazz);
	void fixupMethodRunAddresses(J9Class *ramClass);
	void fixupConstantPool(J9Class *ramClass);
	void fixupClassPathEntries(void);
	void removeUnpersistedClassLoaders(void);
	void saveJ9JavaVMStructures(void);
	bool restorePrimitiveAndArrayClasses(void);
	bool restoreJ9JavaVMStructures(void);
	void savePrimitiveAndArrayClasses(void);
	bool isImmortalClassLoader(J9ClassLoader *classLoader);
	J9MemorySegmentList* copyUnPersistedMemorySegmentsToNewList(J9MemorySegmentList *oldMemorySegmentList);

protected:
	void *operator new(size_t size, void *memoryPointer) { return memoryPointer; }
public:
	VMSnapshotImpl(J9PortLibrary *portLibrary, const char *ramCache);
	~VMSnapshotImpl();

	static VMSnapshotImpl *createInstance(J9PortLibrary *portLibrary, const char *ramCache);

	void setJ9JavaVM(J9JavaVM *vm);
	void setImagePortLib(VMSnapshotImplPortLibrary *vmSnapshotImplPortLibrary);
	J9JavaVM *getJ9JavaVM(void);
	void saveClassLoaderBlocks(void);
	void restoreClassLoaderBlocks(void);
	void saveMemorySegments(void);
	void restoreMemorySegments(void);

	bool setupWarmRun();
	bool setupColdRun();
	bool setupColdRunPostInitialize();

	void fixupJITVtable(J9Class *ramClass);
	void fixupVMStructures(void);
	void teardownImage(void);

	/* Suballocator functions */
	void *subAllocateMemory(uintptr_t byteAmount, bool sub4G);
	void *reallocateMemory(void *address, uintptr_t byteAmount, bool sub4G);
	void freeSubAllocatedMemory(void *memStart, bool sub4G);

	void destroyMonitor(void);
	VMSnapshotImplPortLibrary * getVMSnapshotImplPortLibrary(void) { return _vmSnapshotImplPortLibrary; }

	/* VM Initial Methods accessors/mutators */
	void storeInitialMethods(J9Method *cInitialStaticMethod, J9Method *cInitialSpecialMethod, J9Method *cInitialVirtualMethod);
	void setInitialMethods(J9Method **cInitialStaticMethod, J9Method **cInitialSpecialMethod, J9Method **cInitialVirtualMethod);

	J9ITable *getInvalidITable(void) { return _invalidITable; }
};

#endif /* VMSNAPSHOTIMPLE_HPP_ */
