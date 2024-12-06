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

/*
 * VMSnapshotImpl.hpp
 */

#ifndef VMSNAPSHOTIMPL_HPP_
#define VMSNAPSHOTIMPL_HPP_

#include "j9cfg.h"

#if defined(J9VM_OPT_SNAPSHOTS)

#include "j9.h"
#include "j9comp.h"
#include "j9port_generated.h"
#include "j9protos.h"
#include "objhelp.h"
#include "ut_j9vm.h"
#include "vm_api.h"

/* TODO: Add descriptive doc-comments to the functions herein. */
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
	const char *_vmSnapshotFilePath;
	omrthread_monitor_t _vmSnapshotImplMonitor;
	VMSnapshotImplPortLibrary *_vmSnapshotImplPortLibrary;

public:
	/* TODO: Reallocation will fail, so initial heap size is large (Should be PAGE_SIZE aligned). */
	/* TODO: This initial heap size restriction will be removed once MMAP MAP_FIXED removed (See @ref VMSnapshotImpl::readSnapshotFromFile) */
	static const UDATA GENERAL_MEMORY_SECTION_SIZE = 512 * 1024 * 1024;
	static const UDATA SUB4G_MEMORY_SECTION_SIZE = 100 * 1024 * 1024;
	static const UDATA CLASS_LOADER_REMOVE_COUNT = 8;
	/* Corresponds to the number of J9Class* members refering to primitive
	 * and array classes persisted from the J9JavaVM struct (see SnapshotFileFormat.h).
	 */
	static const UDATA PRIMITIVE_AND_ARRAY_CLASS_COUNT = 17;

private:
	bool initializeMonitor();
	bool initializeInvalidITable();

	void *allocateSnapshotMemory();
	/* TODO: Currently unused. Needed for future ASLR support. */
	void *reallocateSnapshotMemory(UDATA size);
	void *initializeHeap();

	bool readSnapshotFromFile();
	bool writeSnapshotToFile();

	/*
	 * Fixup procedures.
	 */
	void fixupClassLoaders();
	void fixupClasses();
	void fixupClass(J9Class *clazz);
	void fixupArrayClass(J9ArrayClass *clazz);
	void fixupMethodRunAddresses(J9Class *ramClass);
	void fixupConstantPool(J9Class *ramClass);
	void fixupClassPathEntries(J9ClassLoader *classLoader);
	void removeUnpersistedClassLoaders();
	void saveJ9JavaVMStructures();
	void restorePrimitiveAndArrayClasses();
	bool restoreJ9JavaVMStructures();
	void savePrimitiveAndArrayClasses();
	bool isImmortalClassLoader(J9ClassLoader *classLoader);
	void saveHiddenInstanceFields();
	void restoreHiddenInstanceFields();
	J9MemorySegmentList *copyUnPersistedMemorySegmentsToNewList(J9MemorySegmentList *oldMemorySegmentList);

protected:
	void *operator new(size_t size, void *memoryPointer) { return memoryPointer; }
public:
	VMSnapshotImpl(J9PortLibrary *portLibrary, const char *vmSnapshotFilePath);
	~VMSnapshotImpl();

	static VMSnapshotImpl *createInstance(J9PortLibrary *portLibrary, const char *vmSnapshotFilePath);

	void setJ9JavaVM(J9JavaVM *vm);
	void setSnapshotPortLib(VMSnapshotImplPortLibrary *vmSnapshotImplPortLibrary);
	J9JavaVM *getJ9JavaVM();
	void saveClassLoaderBlocks();
	void restoreClassLoaderBlocks();
	void saveMemorySegments();
	void restoreMemorySegments();

	bool setupRestoreRun();
	bool setupSnapshotRun();
	bool setupSnapshotRunPostInitialize();

	void fixupJITVtable(J9Class *ramClass);
	void fixupVMStructures();
	void writeSnapshot();
	void freeJ9JavaVMStructures();

	void *subAllocateMemory(uintptr_t byteAmount, bool sub4G);
	void *reallocateMemory(void *address, uintptr_t byteAmount, bool sub4G);
	void freeSubAllocatedMemory(void *memStart, bool sub4G);

	void destroyMonitor();
	VMSnapshotImplPortLibrary *getVMSnapshotImplPortLibrary() { return _vmSnapshotImplPortLibrary; }

	void storeInitialMethods(J9Method *cInitialStaticMethod, J9Method *cInitialSpecialMethod, J9Method *cInitialVirtualMethod);
	void setInitialMethods(J9Method **cInitialStaticMethod, J9Method **cInitialSpecialMethod, J9Method **cInitialVirtualMethod);

	J9ITable *getInvalidITable() { return _invalidITable; }
};

#endif /* defined(J9VM_OPT_SNAPSHOTS) */

#endif /* VMSNAPSHOTIMPL_HPP_ */
