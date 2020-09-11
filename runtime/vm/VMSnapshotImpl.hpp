/*******************************************************************************
 * Copyright (c) 2001, 2020 IBM Corp. and others
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

#ifndef VMSNAPSHOTIMPLE_HPP_
#define VMSNAPSHOTIMPLE_HPP_

#include "j9cfg.h"

#if defined(J9VM_OPT_SNAPSHOTS)

#include "j9.h"
#include "j9port_generated.h"
#include "j9comp.h"
#include "j9protos.h"
#include "ut_j9vm.h"
#include "objhelp.h"
#include "vm_api.h"
#include "j9.h"

typedef struct {
	OMR_Runtime *omrRuntime;
	OMR_VM *omrVM;
	J9HashTable *registeredNatives;
} VMIntermediateSnapshotState;

class VMSnapshotImpl
{

	/*
	 * Data Members
	 */
private:
	J9JavaVM *_vm;
	J9PortLibrary *_portLibrary;
	J9SnapshotHeader *_snapshotHeader;
	J9MemoryRegion *_memoryRegions;
	J9Heap *_heap;
	J9Heap *_heap32;
	J9ITable *_invalidITable;
	const char *_ramCache;
	const char *_trigger;
	J9Method *_triggerMethod;
	omrthread_monitor_t _vmSnapshotImplMonitor;
	VMSnapshotImplPortLibrary *_vmSnapshotImplPortLibrary;
	intptr_t _snapshotFD;

protected:
public:
	/* TODO: reallocation will fail so initial heap size is large (Should be PAGE_SIZE aligned) */
	/* TODO: initial image size restriction will be removed once MMAP MAP_FIXED removed. see @ref VMSnapshotImpl::readImageFromFile */
	static const UDATA GENERAL_MEMORY_SECTION_SIZE = 512 * 1024 * 1024;
	static const UDATA SUB4G_MEMORY_SECTION_SIZE = 100 * 1024 * 1024;
	static const UDATA CLASS_LOADER_REMOVE_COUNT = 8;

	/*
	 * Function Members
	 */
private:
	bool initializeMonitor(void);
	bool initializeInvalidITable(void);

	/* Image Heap initialization and allocation functions */
	void* allocateImageMemory();
	void* reallocateImageMemory(UDATA size); /* TODO: Function will be used once random memory loading is allowed. Function not used currently */
	void* initializeHeap(void);

	bool allocateImageTableHeaders(void);

	bool readImageFromFile(void);
	bool writeImageToFile(J9VMThread *currentThread);
	bool enterSnapshotMode(J9VMThread *currentThread);
	void leaveSnapshotMode(J9VMThread *currentThread);
	bool registerGCForSnapshot(J9VMThread *currentThread);

	/* Fixup functions called during teardown sequence */
	void fixupClassLoaders(void);
	void fixupClasses(J9VMThread *currentThread);
	void fixupClass(J9VMThread *currentThread, J9Class *clazz);
	void fixupArrayClass(J9ArrayClass *clazz);
	void fixupMethodRunAddresses(J9VMThread *currentThread, J9Class *ramClass);
	void fixupConstantPool(J9Class *ramClass);
	void fixupClassPathEntries(J9ClassLoader *classLoader);
	void removeUnpersistedClassLoaders(void);
	void saveJ9JavaVMStructures(void);
	bool restorePrimitiveAndArrayClasses(void);
	bool restoreJ9JavaVMStructures(void);
	void savePrimitiveAndArrayClasses(void);
	void saveHiddenInstanceFields(void);
	void restoreHiddenInstanceFields(void);
	bool isImmortalClassLoader(J9ClassLoader *classLoader);
	J9MemorySegmentList* copyUnPersistedMemorySegmentsToNewList(J9MemorySegmentList *oldMemorySegmentList);
	void saveJITConfig(void);
	bool preWriteToImage(J9VMThread *currentThread, VMIntermediateSnapshotState *intermediateSnapshotState);
	bool postWriteToImage(J9VMThread *currentThread, VMIntermediateSnapshotState *intermediateSnapshotState);

protected:
	void *operator new(size_t size, void *memoryPointer) { return memoryPointer; }
public:
	VMSnapshotImpl(J9PortLibrary *portLibrary, const char* ramCache, const char* trigger);
	~VMSnapshotImpl();

	static VMSnapshotImpl* createInstance(J9PortLibrary *portLibrary, const char* ramCache, const char* trigger);

	void setJ9JavaVM(J9JavaVM *vm);
	void setImagePortLib(VMSnapshotImplPortLibrary *vmSnapshotImplPortLibrary);
	J9JavaVM* getJ9JavaVM(void);
	void saveClassLoaderBlocks(void);
	void restoreClassLoaderBlocks(void);
	void saveMemorySegments(void);
	void restoreMemorySegments(void);
	bool restoreJITConfig(void);

	bool setupWarmRun();
	bool setupColdRun();
	bool setupMethodTrigger();
	void fixupJITVtable(J9Class *ramClass);
	void fixupVMStructures(void);
	bool writeSnapshotImage(J9VMThread *currentThread);
	bool triggerSnapshot(J9VMThread *currentThread);
	void freeJ9JavaVMStructures(void);

	bool postPortLibInitstage(bool isSnapShotRun);

	/* Suballocator functions */
	void* subAllocateMemory(uintptr_t byteAmount, bool sub4G);
	void* reallocateMemory(void *address, uintptr_t byteAmount, bool sub4G);
	void freeSubAllocatedMemory(void *memStart, bool sub4G);

	void destroyMonitor(void);
	VMSnapshotImplPortLibrary * getVMSnapshotImplPortLibrary(void) { return _vmSnapshotImplPortLibrary; }

	J9ITable* getInvalidITable(void) { return _invalidITable; }
	const char* getSnapshotTrigger(void) { return _trigger; }
	J9Method *getTriggerJ9Method(void) { return _triggerMethod; };
	void setTriggerJ9Method(J9Method *method) { _triggerMethod = method; }
	J9SnapshotHeader* getSnapshotHeader(void) { return _snapshotHeader; }
	J9Heap * getSubAllocatorHeap(void) { return _heap; }

	J9MemoryRegion getGCHeapMemoryRegion() const;

	void setGCHeapMemoryRegion(const J9MemoryRegion *region);

	J9GCSnapshotProperties getGCSnapshotProperties() const;

	void setGCSnapshotProperties(const J9GCSnapshotProperties *properties);

	intptr_t getSnapshotFD() const;
};

#endif /* defined(J9VM_OPT_SNAPSHOTS) */
#endif /* VMSNAPSHOTIMPLE_HPP_ */
