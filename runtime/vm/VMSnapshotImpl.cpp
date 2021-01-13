/*******************************************************************************
 * Copyright (c) 2001, 2021 IBM Corp. and others
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

#include "j9cfg.h"

#if defined(J9VM_OPT_SNAPSHOTS)

#include "VMSnapshotImpl.hpp"
#include "VMHelpers.hpp"
#include "OutOfLineINL.hpp"
#include "SnapshotFileFormat.h"
#include "segment.h"
#include <errno.h>
#include <string.h>
#include "rommeth.h"
#include "util_internal.h"

#include <sys/mman.h> /* TODO: Change to OMRPortLibrary MMAP functionality. Currently does not allow MAP_FIXED as it is not supported in all architectures */


J9_DECLARE_CONSTANT_UTF8(runPostRestoreHooks_sig, "()V");
J9_DECLARE_CONSTANT_UTF8(runPostRestoreHooks_name, "runPostRestoreHooks");

void image_mem_free_memory(struct OMRPortLibrary *portLibrary, void *memoryPointer);
void *image_mem_allocate_memory(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount, const char *callSite, uint32_t category);
void image_mem_free_memory32(struct OMRPortLibrary *portLibrary, void *memoryPointer);
void *image_mem_allocate_memory32(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount, const char *callSite, uint32_t category);

VMSnapshotImpl::VMSnapshotImpl(J9PortLibrary *portLibrary, const char* ramCache, const char* trigger) :
	_vm(NULL),
	_portLibrary(portLibrary),
	_snapshotHeader(NULL),
	_acquiredMonitorHeader(NULL),
	_acquiredMonitors(NULL),
	_memoryRegions(NULL),
	_heap(NULL),
	_heap32(NULL),
	_invalidITable(NULL),
	_ramCache(ramCache),
	_trigger(trigger),
	_triggerMethod(NULL),
	_vmSnapshotImplMonitor(NULL),
	_vmSnapshotImplPortLibrary(NULL),
	_snapshotFD(-1)
{}

void
hookRAMClassLoadForMethodTrigger(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMInternalClassLoadEvent* event = (J9VMInternalClassLoadEvent *)eventData;
	J9VMThread *currentThread = event->currentThread;
	J9JavaVM *vm = currentThread->javaVM;
	J9Class *clazz = event->clazz;
	VMSnapshotImpl *snapshotImpl = (VMSnapshotImpl *) vm->vmSnapshotImplPortLibrary->vmSnapshotImpl;
	J9ROMClass *romClass = clazz->romClass;
	U_32 i;

	J9Method *method = clazz->ramMethods;
	J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClass);
	UDATA classNameLength = J9UTF8_LENGTH(className);
	const char *snapshotTrigger = snapshotImpl->getSnapshotTrigger();
	if (J9UTF8_DATA_EQUALS(J9UTF8_DATA(className), classNameLength, snapshotTrigger, classNameLength)) {
		if ('.' == snapshotTrigger[classNameLength]) {
			const char *methodTrigger = (const char *)(snapshotTrigger +classNameLength + 1);
			for (i = 0; i < romClass->romMethodCount; i++) {
				J9UTF8 *romMethodName = J9ROMMETHOD_NAME(J9_ROM_METHOD_FROM_RAM_METHOD(method));
				UDATA romMethodNameLength = J9UTF8_LENGTH(romMethodName);
				if (J9UTF8_DATA_EQUALS(J9UTF8_DATA(romMethodName), romMethodNameLength, methodTrigger, romMethodNameLength)) {
					snapshotImpl->setTriggerJ9Method(method);
					break;
				}
				method++;
			}
		}
	}
}

void
hookMethodEnterForMethodTrigger(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMMethodEnterEvent* event = (J9VMMethodEnterEvent *)eventData;
	J9VMThread *currentThread = event->currentThread;
	J9JavaVM *vm = currentThread->javaVM;
	VMSnapshotImpl *snapshotImpl = (VMSnapshotImpl *) vm->vmSnapshotImplPortLibrary->vmSnapshotImpl;
	if (event->method == snapshotImpl->getTriggerJ9Method()) {
		if(!snapshotImpl->triggerSnapshot(currentThread)) {
			setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
		}
	}
}

bool
VMSnapshotImpl::triggerSnapshot(J9VMThread *currentThread)
{
	((J9JavaVM*)currentThread->javaVM)->extendedRuntimeFlags2 |= J9_EXTENDED_RUNTIME2_SNAPSHOT_STATE_SNAPSHOT_TRIGGERED;
	return writeSnapshotImage(currentThread);
}

bool
VMSnapshotImpl::setupMethodTrigger()
{
	bool rc = true;
	if (NULL != _trigger) {

		J9HookInterface** hook = _vm->internalVMFunctions->getVMHookInterface(_vm);
		if ((*hook)->J9HookRegisterWithCallSite(hook, J9HOOK_VM_INTERNAL_CLASS_LOAD, hookRAMClassLoadForMethodTrigger, OMR_GET_CALLSITE(), NULL)) {
			rc = false;
			goto done;
		}

		if ((*hook)->J9HookRegisterWithCallSite(hook, J9HOOK_VM_METHOD_ENTER, hookMethodEnterForMethodTrigger, OMR_GET_CALLSITE(), NULL)) {
			rc = false;
			goto done;
		}
	}
done:
	return rc;
}

void
VMSnapshotImpl::setJ9JavaVM(J9JavaVM *vm)
{
	_vm = vm;
}

void
VMSnapshotImpl::setImagePortLib(VMSnapshotImplPortLibrary *vmSnapshotImplPortLibrary)
{
	_vmSnapshotImplPortLibrary = vmSnapshotImplPortLibrary;
}

J9JavaVM *
VMSnapshotImpl::getJ9JavaVM(void)
{
	return _vm;
}

VMSnapshotImpl::~VMSnapshotImpl()
{
	destroyMonitor();

	/* We can only clean up if the _vm is available.
	 * If it's not, then we don't know what state we're
	 * in.  Better to shutdown without crashing than to
	 * try and cleanup bits and pieces
	 */
	if (NULL != _vm) {
		PORT_ACCESS_FROM_JAVAVM(_vm);
		if (IS_SNAPSHOT_RUN(_vm)) {
			for (int i = 0; i < NUM_OF_MEMORY_SECTIONS; i++) {
				const J9MemoryRegion *region = &_memoryRegions[i];
				switch (region->type) {
				case GENERAL:
					munmap(region->alignedStartAddr, region->mappableSize);
					break;
				case SUB4G:
					j9mem_free_memory32(region->startAddr);
					break;
				case GC_HEAP:
					/* skip */
					break;
				default:
					j9mem_free_memory(region->startAddr);
					break;
				}
			}
		} else {
			for (int i = 0; i < NUM_OF_MEMORY_SECTIONS; i++) {
				munmap(_memoryRegions[i].alignedStartAddr, _memoryRegions[i].mappableSize);
			}
		}
		j9mem_free_memory((void *)_memoryRegions);
		j9mem_free_memory((void *)_acquiredMonitors);
		j9mem_free_memory((void *)_acquiredMonitorHeader);
		j9mem_free_memory((void *)_snapshotHeader);
	}
}

bool
VMSnapshotImpl::initializeMonitor(void)
{
	if (omrthread_monitor_init_with_name(&_vmSnapshotImplMonitor, 0, "JVM Image Heap Monitor") != 0) {
		return false;
	}

	return true;
}

bool
VMSnapshotImpl::initializeInvalidITable(void)
{
	_invalidITable = (J9ITable *) subAllocateMemory(sizeof(J9ITable), J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm));
	if (NULL == _invalidITable) {
		return false;
	}

	_invalidITable->interfaceClass = (J9Class *) (UDATA) 0xDEADBEEF;
	_invalidITable->depth = 0;
	_invalidITable->next = (J9ITable *) NULL;

	return true;
}

void
VMSnapshotImpl::destroyMonitor(void)
{
	if (NULL != _vmSnapshotImplMonitor) {
		omrthread_monitor_destroy(_vmSnapshotImplMonitor);
		_vmSnapshotImplMonitor = NULL;
	}
}

VMSnapshotImpl *
VMSnapshotImpl::createInstance(J9PortLibrary *portLibrary, const char* ramCache, const char* trigger)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	VMSnapshotImpl *jvmInstance = (VMSnapshotImpl *)j9mem_allocate_memory(sizeof(VMSnapshotImpl), J9MEM_CATEGORY_CLASSES);
	if (NULL != jvmInstance) {
		new(jvmInstance) VMSnapshotImpl(portLibrary, ramCache, trigger);
	}

	return jvmInstance;
}

bool
VMSnapshotImpl::setupWarmRun(void)
{
	bool success = true;
	if (!readImageFromFile()) {
		success = false;
		goto done;
	}

	if (!initializeMonitor()) {
		success = false;
		goto done;
	}

	if (!restoreJ9JavaVMStructures()) {
		success = false;
		goto done;
	}

done:
	return success;
}

bool
VMSnapshotImpl::setupColdRun(void)
{
	bool success = true;
	if (NULL == allocateImageMemory()) {
		success = false;
		goto done;
	}

	if (NULL == initializeHeap()) {
		success = false;
		goto done;
	}

	if (!initializeMonitor()) {
		success = false;
		goto done;
	}

done:
	return success;
}

void *
VMSnapshotImpl::allocateImageMemory()
{
	PORT_ACCESS_FROM_PORT(_portLibrary);
	UDATA pageSize = j9vmem_supported_page_sizes()[0];
	void *generalMemorySection = NULL;
	void *sub4GBMemorySection = NULL;

	_snapshotHeader = (J9SnapshotHeader *) j9mem_allocate_memory(sizeof(J9SnapshotHeader), J9MEM_CATEGORY_CLASSES);
	if (NULL == _snapshotHeader) {
		goto done;
	}

	_snapshotHeader->numOfMemorySections = NUM_OF_MEMORY_SECTIONS;

	_acquiredMonitorHeader = (J9AcquiredMonitorHeader *) j9mem_allocate_memory(sizeof(J9AcquiredMonitorHeader), J9MEM_CATEGORY_CLASSES);
	if (NULL == _acquiredMonitorHeader) {
		goto freeHeader;
	}

	_acquiredMonitorHeader->numOfAcquiredMonitors = 0;

	_acquiredMonitors = (J9AcquiredMonitor *) j9mem_allocate_memory(sizeof(J9AcquiredMonitor) * MAX_NUM_ALLOCATED_MONITORS, J9MEM_CATEGORY_CLASSES);
	if (NULL == _acquiredMonitors) {
		goto freeAcquiredMonitorHeader;
	}

	_memoryRegions = (J9MemoryRegion *) j9mem_allocate_memory(sizeof(J9MemoryRegion) * NUM_OF_MEMORY_SECTIONS, J9MEM_CATEGORY_CLASSES);
	if (NULL == _memoryRegions) {
		goto freeAcquiredMonitors;
	}

	//generalMemorySection = j9mem_allocate_memory(GENERAL_MEMORY_SECTION_SIZE + pageSize, J9MEM_CATEGORY_CLASSES);
	generalMemorySection = mmap(
			0,
			GENERAL_MEMORY_SECTION_SIZE + pageSize,
			PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (NULL == generalMemorySection) {
		goto freeMemorySections;
	}

	_memoryRegions[GENERAL].startAddr = generalMemorySection;
	_memoryRegions[GENERAL].alignedStartAddr = (void *) ROUND_UP_TO_POWEROF2((UDATA)generalMemorySection, pageSize);
	_memoryRegions[GENERAL].totalSize = GENERAL_MEMORY_SECTION_SIZE + pageSize;
	_memoryRegions[GENERAL].mappableSize = GENERAL_MEMORY_SECTION_SIZE;
	_memoryRegions[GENERAL].permissions = PROT_READ | PROT_WRITE | PROT_EXEC;
	_memoryRegions[GENERAL].type = GENERAL;

	sub4GBMemorySection = j9mem_allocate_memory32(SUB4G_MEMORY_SECTION_SIZE + pageSize, J9MEM_CATEGORY_CLASSES);
	if (NULL == sub4GBMemorySection) {
		goto freeGeneralMemorySection;
	}

	_memoryRegions[SUB4G].startAddr = sub4GBMemorySection;
	_memoryRegions[SUB4G].alignedStartAddr = (void *) ROUND_UP_TO_POWEROF2((UDATA)sub4GBMemorySection, pageSize);
	_memoryRegions[SUB4G].totalSize = SUB4G_MEMORY_SECTION_SIZE + pageSize;
	_memoryRegions[SUB4G].mappableSize = SUB4G_MEMORY_SECTION_SIZE;
	_memoryRegions[SUB4G].permissions = PROT_READ | PROT_WRITE;
	_memoryRegions[SUB4G].type = SUB4G;

done:
	return _snapshotHeader;

freeGeneralMemorySection:
	j9mem_free_memory(generalMemorySection);

freeMemorySections:
	j9mem_free_memory(_memoryRegions);
	_memoryRegions = NULL;

freeAcquiredMonitors:
	j9mem_free_memory(_acquiredMonitors);
	_acquiredMonitors = NULL;

freeAcquiredMonitorHeader:
	j9mem_free_memory(_acquiredMonitorHeader);
	_acquiredMonitorHeader = NULL;

freeHeader:
	j9mem_free_memory(_snapshotHeader);
	_snapshotHeader = NULL;

	goto done;
}

/* TODO: Currently reallocating image memory is broken since all references to memory inside of heap will fail (i.e. vm->classLoadingPool) */
/* TODO: Better approach is to be able to deal with multiple suballocator regions */
void *
VMSnapshotImpl::reallocateImageMemory(UDATA size)
{
	Assert_VM_unreachable();
	return NULL;
}

void *
VMSnapshotImpl::initializeHeap(void)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);

	_heap = j9heap_create((J9Heap *)_memoryRegions[GENERAL].alignedStartAddr, _memoryRegions[GENERAL].mappableSize, 0);
	if (NULL == _heap) {
		return NULL;
	}

	_heap32 = j9heap_create((J9Heap *)_memoryRegions[SUB4G].alignedStartAddr, _memoryRegions[SUB4G].mappableSize, 0);
	if (NULL == _heap32) {
		return NULL;
	}

	return _heap;
}

void *
VMSnapshotImpl::subAllocateMemory(uintptr_t byteAmount, bool sub4G)
{
	Trc_VM_SubAllocateImageMemory_Entry(_snapshotHeader, byteAmount);

	J9Heap *heap = sub4G ? _heap32 : _heap;

	omrthread_monitor_enter(_vmSnapshotImplMonitor);

	OMRPortLibrary *portLibrary = VMSNAPSHOTIMPL_OMRPORT_FROM_VMSNAPSHOTIMPLPORT(_vmSnapshotImplPortLibrary);
	void *memStart = portLibrary->heap_allocate(portLibrary, heap, byteAmount);
	/* image memory is not large enough and needs to be reallocated */
	if (NULL == memStart) {
		reallocateImageMemory(_snapshotHeader->imageSize * 2 + byteAmount); /* guarantees size of heap will accomodate byteamount */
		memStart = portLibrary->heap_allocate(portLibrary, heap, byteAmount);
	}

	omrthread_monitor_exit(_vmSnapshotImplMonitor);

	Trc_VM_SubAllocateImageMemory_Exit(memStart);

	return memStart;
}

void*
VMSnapshotImpl::reallocateMemory(void *address, uintptr_t byteAmount, bool sub4G)
{
	J9Heap *heap = sub4G ? _heap32 : _heap;

	omrthread_monitor_enter(_vmSnapshotImplMonitor);

	OMRPortLibrary *portLibrary = VMSNAPSHOTIMPL_OMRPORT_FROM_VMSNAPSHOTIMPLPORT(_vmSnapshotImplPortLibrary);
	void *memStart = portLibrary->heap_reallocate(portLibrary, heap, address, byteAmount);
	/* image memory is not large enough and needs to be reallocated */
	if (NULL == memStart) {
		reallocateImageMemory(_snapshotHeader->imageSize * 2 + byteAmount); /* guarantees size of heap will accomodate byteamount */
		memStart = portLibrary->heap_reallocate(portLibrary, heap, address, byteAmount);
	}

	omrthread_monitor_exit(_vmSnapshotImplMonitor);

	return memStart;
}

void
VMSnapshotImpl::freeSubAllocatedMemory(void *address, bool sub4G)
{
	J9Heap *heap = sub4G ? _heap32 : _heap;

	omrthread_monitor_enter(_vmSnapshotImplMonitor);

	OMRPortLibrary *portLibrary = VMSNAPSHOTIMPL_OMRPORT_FROM_VMSNAPSHOTIMPLPORT(_vmSnapshotImplPortLibrary);
	portLibrary->heap_free(portLibrary, heap, address);

	omrthread_monitor_exit(_vmSnapshotImplMonitor);
}

void
VMSnapshotImpl::saveClassLoaderBlocks(void)
{
	_snapshotHeader->savedJavaVMStructs.classLoaderBlocks = _vm->classLoaderBlocks;
	_snapshotHeader->savedJavaVMStructs.systemClassLoader = _vm->systemClassLoader;
	_snapshotHeader->savedJavaVMStructs.extensionClassLoader = _vm->extensionClassLoader;
	_snapshotHeader->savedJavaVMStructs.applicationClassLoader = _vm->applicationClassLoader;
	_snapshotHeader->savedJavaVMStructs.anonClassLoader = _vm->anonClassLoader;
}

void
VMSnapshotImpl::restoreClassLoaderBlocks(void)
{
	/* TRIGGER_J9HOOK_VM_CLASS_LOADER_CREATED is currently unused, if this changes we
	 * can not simply restore the classloaders
	 */
	_vm->classLoaderBlocks = _snapshotHeader->savedJavaVMStructs.classLoaderBlocks;
	_vm->systemClassLoader = _snapshotHeader->savedJavaVMStructs.systemClassLoader;
	_vm->extensionClassLoader = _snapshotHeader->savedJavaVMStructs.extensionClassLoader;
	_vm->applicationClassLoader = _snapshotHeader->savedJavaVMStructs.applicationClassLoader;
	_vm->anonClassLoader = _snapshotHeader->savedJavaVMStructs.anonClassLoader;
}

bool
VMSnapshotImpl::isImmortalClassLoader(J9ClassLoader *classLoader)
{
	if ((classLoader == _vm->applicationClassLoader)
	|| (classLoader == _vm->systemClassLoader)
	|| (classLoader == _vm->extensionClassLoader)
	|| (NULL == classLoader)
	) {
		return true;
	}

	return false;
}

void
printAllSegments(J9MemorySegmentList *segmentList, J9JavaVM *_vm)
{
	J9MemorySegment *currentSegment = segmentList->nextSegment;
	int i = 0;

	if (_vm->classMemorySegments == segmentList) {
		printf("class memory segments ");
	}
	printf("Segment %p Total segments ------------ \n", segmentList);

	while (NULL != currentSegment) {
		printf("segment=%p heapBase=%p size=%d\n", currentSegment, currentSegment->heapBase, (int)currentSegment->size);
		i++;
		currentSegment = currentSegment->nextSegment;
	}
	printf("%p Total segments %d -------------\n", segmentList, (int) i);
}

J9MemorySegmentList*
VMSnapshotImpl::copyUnPersistedMemorySegmentsToNewList(J9MemorySegmentList *oldMemorySegmentList)
{
	J9MemorySegment *currentSegment = oldMemorySegmentList->nextSegment;
	J9MemorySegmentList *newMemorySegmentList = _vm->internalVMFunctions->allocateMemorySegmentList(_vm, 10, OMRMEM_CATEGORY_VM);

	if (NULL == newMemorySegmentList) {
		_vm->internalVMFunctions->setNativeOutOfMemoryError(currentVMThread(_vm), 0 ,0);
		goto done;
	}

	/* copy all the persisted memory segments into the new list */
	while (NULL != currentSegment) {
		J9MemorySegment *nextSegment = currentSegment->nextSegment;

		if (!IS_SEGMENT_PERSISTED(currentSegment) || !isImmortalClassLoader(currentSegment->classLoader)) {
			J9MemorySegment *newSegment = allocateMemorySegmentListEntry(newMemorySegmentList);

			newSegment->type = currentSegment->type;
			newSegment->size = currentSegment->size;
			newSegment->baseAddress = currentSegment->baseAddress;
			newSegment->heapBase = currentSegment->heapBase;
			newSegment->heapTop = currentSegment->heapTop;
			newSegment->heapAlloc = currentSegment->heapAlloc;

			newMemorySegmentList->totalSegmentSize += newSegment->size;
			if (0 != (newMemorySegmentList->flags & MEMORY_SEGMENT_LIST_FLAG_SORT)) {
				avl_insert(&newMemorySegmentList->avlTreeData, (J9AVLTreeNode *) newSegment);
			}

			/* remove segment from old list */
			if (0 != (oldMemorySegmentList->flags & MEMORY_SEGMENT_LIST_FLAG_SORT)) {
				avl_delete(&oldMemorySegmentList->avlTreeData, (J9AVLTreeNode *) currentSegment);
			}
			J9_MEMORY_SEGMENT_LINEAR_LINKED_LIST_REMOVE(oldMemorySegmentList->nextSegment, currentSegment);
			pool_removeElement(oldMemorySegmentList->segmentPool, currentSegment);
		}
		currentSegment = nextSegment;
	}

done:
	return newMemorySegmentList;
}

void
VMSnapshotImpl::saveMemorySegments(void)
{
	/* The approach here is to copy unpersisted segments to a new list, and free those.
	 * The persisted segments will be stored in the image.
	 */
	_snapshotHeader->savedJavaVMStructs.classMemorySegments = _vm->classMemorySegments;
	_vm->classMemorySegments = copyUnPersistedMemorySegmentsToNewList(_vm->classMemorySegments);

	_snapshotHeader->savedJavaVMStructs.memorySegments = _vm->memorySegments;
	_vm->memorySegments = copyUnPersistedMemorySegmentsToNewList(_vm->memorySegments);

	return;
}

void
VMSnapshotImpl::restoreMemorySegments(void)
{
	_vm->memorySegments = _snapshotHeader->savedJavaVMStructs.memorySegments;
	_vm->classMemorySegments = _snapshotHeader->savedJavaVMStructs.classMemorySegments;
}


bool
VMSnapshotImpl::restoreJITConfig(void)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);

	bool rc = true;
	J9JITConfig *jitConfig = _vm->jitConfig;

	if (NULL != jitConfig) {
		if (omrthread_monitor_init_with_name(&jitConfig->thunkHashTableMutex, 0, "JIT thunk table")) {
			j9tty_printf(PORTLIB, "failed to initialize jitConfig->thunkHashTableMutex");
			rc = false;
			goto done;
		}

		jitConfig->codeCacheList = NULL;
		jitConfig->dataCacheList = NULL;
		jitConfig->privateConfig = NULL;
		jitConfig->scratchSegment = NULL;
	}

done:
	return rc;
}


void
VMSnapshotImpl::savePrimitiveAndArrayClasses(void)
{
	J9Class **saveClasses = &_snapshotHeader->savedJavaVMStructs.voidReflectClass;
	J9Class **liveClasses = &_vm->voidReflectClass;

	for (int i = 0; i < 17; i++) {
		saveClasses[i] = liveClasses[i];
	}
}

bool
VMSnapshotImpl::restorePrimitiveAndArrayClasses(void)
{
	J9Class **saveClasses = &_snapshotHeader->savedJavaVMStructs.voidReflectClass;
	J9Class **liveClasses = &_vm->voidReflectClass;
	bool rc = true;

	for (int i = 0; i < 17; i++) {
		liveClasses[i] = saveClasses[i];
	}

	return rc;
}




void
VMSnapshotImpl::fixupVMStructures(void)
{
	UDATA omrRuntimeOffset = ROUND_UP_TO_POWEROF2(sizeof(J9JavaVM), 8);
	UDATA omrVMOffset = ROUND_UP_TO_POWEROF2(omrRuntimeOffset + sizeof(OMR_Runtime), 8);
	UDATA vmAllocationSize = omrVMOffset + sizeof(OMR_VM);
	memset(_vm + 1, 0, vmAllocationSize - sizeof(J9JavaVM));
	_vm->memoryManagerFunctions = NULL;
	_vm->j9rasGlobalStorage = NULL;
	_vm->lockwordExceptions = NULL;
	_vm->zipCachePool = NULL;
	_vm->sharedCacheAPI = NULL;
	_vm->deadThreadList = NULL;
	_vm->exclusiveAccessState = J9_XACCESS_NONE;
	_vm->attachContext.semaphore = NULL;
	_vm->flushMutex = NULL;
}

void
VMSnapshotImpl::fixupClassLoaders(void)
{
	pool_state classLoaderWalkState = {0};
	J9ClassLoader *currentClassLoader = (J9ClassLoader *) pool_startDo(_vm->classLoaderBlocks, &classLoaderWalkState);
	while (NULL != currentClassLoader) {
		currentClassLoader->jniRedirectionBlocks = NULL;
		currentClassLoader->sharedLibraries = NULL;
		currentClassLoader->librariesHead = NULL;
		currentClassLoader->librariesTail = NULL;
		currentClassLoader->gcLinkNext = NULL;
		currentClassLoader->gcLinkPrevious = NULL;
		fixupClassPathEntries(currentClassLoader);
		currentClassLoader = (J9ClassLoader *) pool_nextDo(&classLoaderWalkState);
	}
}

void
VMSnapshotImpl::fixupClassPathEntries(J9ClassLoader *classLoader)
{
	J9ClassPathEntry *cpEntries = classLoader->classPathEntries;
	UDATA classPathEntryCount = classLoader->classPathEntryCount;

	for (UDATA i = 0; i < classPathEntryCount; i++) {
		cpEntries[i].extraInfo = NULL;
	}
}

void
VMSnapshotImpl::removeUnpersistedClassLoaders(void)
{
	/* wont need this once we support all classloaders */
	pool_state classLoaderWalkState = {0};
	UDATA numOfClassLoaders = pool_numElements(_vm->classLoaderBlocks);
	U_8 buf[CLASS_LOADER_REMOVE_COUNT * sizeof(J9ClassLoader*)];
	J9ClassLoader **removeLoaders = (J9ClassLoader **) buf;
	J9VMThread *vmThread = currentVMThread(_vm);
	UDATA count = 0;
	PORT_ACCESS_FROM_PORT(_portLibrary);

	if (CLASS_LOADER_REMOVE_COUNT < numOfClassLoaders) {

		removeLoaders = (J9ClassLoader **) j9mem_allocate_memory(numOfClassLoaders * sizeof(J9ClassLoader*), J9MEM_CATEGORY_CLASSES);
		if (NULL == removeLoaders) {
			/* TODO error handling for fixups */
			Assert_VM_unreachable();
		}
	}

	J9ClassLoader *classloader = (J9ClassLoader *) pool_startDo(_vm->classLoaderBlocks, &classLoaderWalkState);
	while (NULL != classloader) {
		if (!isImmortalClassLoader(classloader)) {
			removeLoaders[count] = classloader;
			count++;
		}
		classloader = (J9ClassLoader *) pool_nextDo(&classLoaderWalkState);
	}

	for (UDATA i = 0; i < count; i++) {
		freeClassLoader(removeLoaders[i], _vm, vmThread, FALSE);
	}

	if ((J9ClassLoader **)buf != removeLoaders) {
		j9mem_free_memory(removeLoaders);
	}
}


void
VMSnapshotImpl::fixupClasses(J9VMThread *currentThread)
{
	pool_state classLoaderWalkState = {0};
	J9ClassLoader *classloader = (J9ClassLoader *) pool_startDo(_vm->classLoaderBlocks, &classLoaderWalkState);
	while (NULL != classloader) {
		J9ClassWalkState walkState = {0};
		J9Class *currentClass = allLiveClassesStartDo(&walkState, _vm, classloader);

		while (NULL != currentClass) {
			J9ROMClass *romClass = currentClass->romClass;

			if (!J9ROMCLASS_IS_ARRAY(romClass)) {
				fixupClass(currentThread, currentClass);
			}

			currentClass = allLiveClassesNextDo(&walkState);
		}
		allLiveClassesEndDo(&walkState);

		classloader = (J9ClassLoader *) pool_nextDo(&classLoaderWalkState);
	}


}

void
VMSnapshotImpl::fixupClass(J9VMThread *currentThread, J9Class *clazz)
{
	fixupMethodRunAddresses(currentThread, clazz);
	clazz->customSpinOption = NULL;
}

void
VMSnapshotImpl::fixupJITVtable(J9Class *ramClass)
{
	J9VMThread *vmThread = currentVMThread(_vm);
	J9JITConfig *jitConfig = _vm->jitConfig;
	J9VTableHeader *vTableAddress = J9VTABLE_HEADER_FROM_RAM_CLASS(ramClass);
	if (jitConfig != NULL) {
		UDATA *vTableWriteCursor = JIT_VTABLE_START_ADDRESS(ramClass);
		UDATA vTableWriteIndex = vTableAddress->size;
		J9Method **vTableReadCursor;
		if (vTableWriteIndex != 0) {
			if ((jitConfig->runtimeFlags & J9JIT_TOSS_CODE) != 0) {
				vTableWriteCursor -= vTableWriteIndex;
			} else {

				vTableReadCursor = J9VTABLE_FROM_HEADER(vTableAddress);
				for (; vTableWriteIndex > 0; vTableWriteIndex--) {
					J9Method *currentMethod = *vTableReadCursor;
					fillJITVTableSlot(vmThread, vTableWriteCursor, currentMethod);

					vTableReadCursor++;
					vTableWriteCursor--;
				}
			}
		}
	}
}

void
VMSnapshotImpl::fixupArrayClass(J9ArrayClass *clazz)
{
	clazz->classObject = NULL;
	fixupConstantPool((J9Class *) clazz);
	clazz->initializeStatus = J9ClassInitSucceeded;
	clazz->jniIDs = NULL;
	clazz->replacedClass = NULL;
	clazz->callSites = NULL;
	clazz->methodTypes = NULL;
	clazz->varHandleMethodTypes = NULL;
	clazz->gcLink = NULL;

	UDATA i;
	if (NULL != clazz->staticSplitMethodTable) {
		for (i = 0; i < clazz->romClass->staticSplitMethodRefCount; i++) {
			clazz->staticSplitMethodTable[i] = (J9Method*)_vm->initialMethods.initialStaticMethod;
		}
	}
	if (NULL != clazz->specialSplitMethodTable) {
		for (i = 0; i < clazz->romClass->specialSplitMethodRefCount; i++) {
			clazz->specialSplitMethodTable[i] = (J9Method*)_vm->initialMethods.initialSpecialMethod;
		}
	}
}

void
VMSnapshotImpl::fixupMethodRunAddresses(J9VMThread *currentThread, J9Class *ramClass)
{
	J9ROMClass *romClass = ramClass->romClass;

	/* for all ramMethods call initializeMethodRunAdress for special methods */
	if (romClass->romMethodCount != 0) {
		UDATA i;
		UDATA count = romClass->romMethodCount;
		J9Method *ramMethod = ramClass->ramMethods;
		for (i = 0; i < count; i++) {
			if (J9_BCLOOP_SEND_TARGET_OUT_OF_LINE_INL == J9_BCLOOP_DECODE_SEND_TARGET(ramMethod->methodRunAddress)
				|| J9_BCLOOP_SEND_TARGET_COUNT_AND_SEND_JNI_NATIVE == J9_BCLOOP_DECODE_SEND_TARGET(ramMethod->methodRunAddress)
				|| J9_BCLOOP_SEND_TARGET_COMPILE_JNI_NATIVE == J9_BCLOOP_DECODE_SEND_TARGET(ramMethod->methodRunAddress)
				|| J9_BCLOOP_SEND_TARGET_RUN_JNI_NATIVE == J9_BCLOOP_DECODE_SEND_TARGET(ramMethod->methodRunAddress)
			) {
				initializeMethodRunAddressImpl(currentThread, ramMethod, FALSE, TRUE);
				ramMethod->extra = (void*)(UDATA)J9_STARTPC_NOT_TRANSLATED;
			}

			ramMethod++;
		}
	}
}

void
VMSnapshotImpl::fixupConstantPool(J9Class *ramClass)
{
	J9VMThread *vmThread = currentVMThread(_vm);
	J9ROMClass *romClass = ramClass->romClass;
	J9ConstantPool *ramCP = ((J9ConstantPool *) ramClass->ramConstantPool);
	J9ConstantPool *ramCPWithoutHeader = ramCP + 1;
	UDATA ramCPCount = romClass->ramConstantPoolCount;
	UDATA ramCPCountWithoutHeader = ramCPCount - 1;

	/* Zero the ramCP and initialize constant pool */
	if (ramCPCount != 0) {
		memset(ramCPWithoutHeader, 0, ramCPCountWithoutHeader * sizeof(J9RAMConstantPoolItem));
		internalRunPreInitInstructions(ramClass, vmThread);
	}
}

bool
VMSnapshotImpl::readImageFromFile(void)
{
	bool rc = true;
	Trc_VM_ReadImageFromFile_Entry(_heap, _ramCache);
	PORT_ACCESS_FROM_PORT(_portLibrary);

	OMRPORT_ACCESS_FROM_OMRPORT(&_portLibrary->omrPortLibrary);

	_snapshotFD = omrfile_open(_ramCache, EsOpenRead, 0444);
	if (-1 == _snapshotFD) {
		j9tty_printf(PORTLIB, "failed to open ramCache file=%s errno=%d\n", _ramCache, errno);
		rc = false;
		goto done;
	}

	_snapshotHeader = (J9SnapshotHeader *) j9mem_allocate_memory(sizeof(J9SnapshotHeader), J9MEM_CATEGORY_CLASSES);
	if (NULL == _snapshotHeader) {
		rc = false;
		goto done;
	}

	_acquiredMonitorHeader = (J9AcquiredMonitorHeader *) j9mem_allocate_memory(sizeof(J9AcquiredMonitorHeader), J9MEM_CATEGORY_CLASSES);
	if (NULL == _acquiredMonitorHeader) {
		rc = false;
		goto freeSnapshotHeader;
	}

	_acquiredMonitors = (J9AcquiredMonitor *) j9mem_allocate_memory(sizeof(J9AcquiredMonitor) * MAX_NUM_ALLOCATED_MONITORS, J9MEM_CATEGORY_CLASSES);
	if (NULL == _acquiredMonitors) {
		rc = false;
		goto freeAcquiredMonitorHeader;
	}

	_memoryRegions = (J9MemoryRegion *) j9mem_allocate_memory(sizeof(J9MemoryRegion) * NUM_OF_MEMORY_SECTIONS, J9MEM_CATEGORY_CLASSES);
	if (NULL == _memoryRegions) {
		rc = false;
		goto freeAcquiredMonitors;
	}

	/* Read snapshot header and memory regions then mmap the rest of the image (heap) into memory */
	if (-1 == omrfile_read(_snapshotFD, (void *)_snapshotHeader, sizeof(J9SnapshotHeader))) {
		rc = false;
		goto freeMemorySections;

	}

	if (-1 == omrfile_read(_snapshotFD, (void *)_acquiredMonitorHeader, sizeof(J9AcquiredMonitorHeader))) {
		rc = false;
		goto freeMemorySections;
	}

	if (-1 == omrfile_read(_snapshotFD, (void *)_acquiredMonitors, sizeof(J9AcquiredMonitor) * MAX_NUM_ALLOCATED_MONITORS)) {
		rc = false;
		goto freeMemorySections;
	}

	if (-1 == omrfile_read(_snapshotFD, (void *)_memoryRegions, sizeof(J9MemoryRegion) * NUM_OF_MEMORY_SECTIONS)) {
		rc = false;
		goto freeMemorySections;
	}

	for (int i = 0; i < NUM_OF_MEMORY_SECTIONS; i++) {
		if (i == GC_HEAP) continue;

		void *mappedSection = mmap(
				(void *)_memoryRegions[i].alignedStartAddr,
				_memoryRegions[i].mappableSize,
				_memoryRegions[i].permissions, MAP_PRIVATE, _snapshotFD,
				_memoryRegions[i].fileOffset);
		if ((MAP_FAILED == mappedSection) || (_memoryRegions[i].alignedStartAddr != mappedSection)) {
			j9tty_printf(PORTLIB, "map failed: fileOffset=%d alignedStartAddr=%p errno=%d\n", _memoryRegions[i].fileOffset, _memoryRegions[i].alignedStartAddr, errno);
			rc = false;
			goto freeMemorySections;
		}
	}

	_heap = (J9Heap *)_memoryRegions[GENERAL].alignedStartAddr;
	_heap32 = (J9Heap *)_memoryRegions[SUB4G].alignedStartAddr;
	_vm = _snapshotHeader->vm;
	_vm->ramStateFilePath = (char*) _ramCache;

#if defined(DEBUG)
	{
		OMRPORT_ACCESS_FROM_J9PORT(_portLibrary);
		const J9MemoryRegion *gcRegion = &_memoryRegions[GC_HEAP];
		omrtty_err_printf("==== GC Region Stats ====\n");
		omrtty_err_printf("heapBase    = %p\n",  (void*) gcRegion->alignedStartAddr);
		omrtty_err_printf("heapMinSize = %zu\n", (size_t)gcRegion->totalSize);
		omrtty_err_printf("heapMaxSize = %zu\n", (size_t)gcRegion->mappableSize);
		omrtty_err_printf("=========================\n");
	}
#endif /* defined(DEBUG) */

done:
	Trc_VM_ReadImageFromFile_Exit();
	return rc;

freeMemorySections: /* free all allocated memory */
	j9mem_free_memory(_memoryRegions);
	_memoryRegions = NULL;

freeAcquiredMonitors:
	j9mem_free_memory(_acquiredMonitors);
	_acquiredMonitors = NULL;

freeAcquiredMonitorHeader:
	j9mem_free_memory(_acquiredMonitorHeader);
	_acquiredMonitorHeader = NULL;

freeSnapshotHeader:
	j9mem_free_memory(_snapshotHeader);
	_snapshotHeader = NULL;

	goto done;
}

void
VMSnapshotImpl::saveHiddenInstanceFields(void)
{
	_snapshotHeader->savedJavaVMStructs.hiddenInstanceFields = _vm->hiddenInstanceFields;
}

void
VMSnapshotImpl::saveJITConfig(void)
{
	if (NULL != _vm->jitConfig) {
		_vm->jitConfig->imageCodeCacheStart = _vm->jitConfig->codeCache->heapBase;
		_vm->jitConfig->imageCodeCacheEnd = _vm->jitConfig->codeCache->heapTop;
	}
}

void
VMSnapshotImpl::restoreHiddenInstanceFields(void)
{
	_vm->hiddenInstanceFields = _snapshotHeader->savedJavaVMStructs.hiddenInstanceFields;
}

/**
 * Will be removed once the J9JavaVM struct is fully persisted
 */
void
VMSnapshotImpl::saveJ9JavaVMStructures(void)
{
	saveJITConfig();

	_snapshotHeader->vm = _vm;
	_snapshotHeader->savedJavaVMStructs.vmSnapshotImplPortLibrary = _vm->vmSnapshotImplPortLibrary;
}

/**
 * If monitor is acquired by thread save fixup information for image restore.
 * 
 * @param thread J9VMThread that may have acquired the monitor
 * @param cursor next empty spot in allocated snapshot memory for acquired monitors. Will be incremented if slot is filled
 * @param monitor monitor to save if acquired by thread
 * @param isObjectMonitor
 * @param fixupReference
 * 
 * @return true if successful, else false
 */
bool
VMSnapshotImpl::saveAcquiredMonitor(J9VMThread *thread, J9AcquiredMonitor **cursor, omrthread_monitor_t monitor, UDATA isObjectMonitor, UDATA fixupReference)
{
	bool success = true;

	if (omrthread_monitor_is_acquired(monitor)) {
		omrthread_t ownerThreadOmr = omrthread_monitor_get_current_owner(monitor);

		if (thread->osThread == ownerThreadOmr) {

			/* monitor should be persisted. verify there is enough space allocated. */
			if (MAX_NUM_ALLOCATED_MONITORS == _acquiredMonitorHeader->numOfAcquiredMonitors) {
				printf("Error: out of space for persisted monitors\n");
				success = false;
				goto done;
			}

			(*cursor)->isObjectMonitor = isObjectMonitor;
			(*cursor)->fixupReference = fixupReference;
			(*cursor)->ownerCount = omrthread_monitor_get_acquired_count(monitor);
			(*cursor)->ownerVmThreadAddress = (UDATA) thread;

			_acquiredMonitorHeader->numOfAcquiredMonitors++;
			*cursor = *cursor + 1;
		}
	}

done:
	return success;
}

/**
 * Find fixup id for monitor. Return 0 is monitor
 * has no matching id meaning don't bother to persist. 
 * @param monitor
 * @return UDATA fixupId
 */
UDATA
VMSnapshotImpl::systemMonitorToFixupId(omrthread_monitor_t monitor) {
	UDATA fixupId = 0;
	if (monitor == _vm->unsafeMemoryTrackingMutex) {
		fixupId = FIXUPREFVM_UNSAFE_MEMORY_TRACKING_MUTEX;
	} else if (monitor == _vm->verboseStateMutex) {
		fixupId = FIXUPREFVM_VERBOSE_STATE_MUTEX;
	} else if (monitor == _vm->jclCacheMutex) {
		fixupId = FIXUPREFVM_JCL_CACHE_MUTEX;
	} else if (monitor == _vm->constantDynamicMutex) {
		fixupId= FIXUPREFVM_CONSTANT_DYNAMIC_MUTEX;
	}
	return fixupId;
}

/**
 * If thread is waiting to acquire a monitor save fixup
 * information for restore. Any monitor that is waiting
 * to be acquired should already have been saved in
 * _acquiredMonitorHeader for its owner to acquire on restore.
 * @param thread
 */
void
VMSnapshotImpl::saveSystemMonitorWaiting(J9VMThread *thread) {
	omrthread_monitor_t monitor = omrthread_waiting_to_acquire(thread->osThread);
	if (NULL != monitor) {
		thread->waitingMonitorFixupId = systemMonitorToFixupId(monitor);
	}
}

/** 
 * At this stage the VM is at a standstill so its safe to start recording lock information.
 * 
 * OMR level threads and mutexes are not saved by the snapshot suballocator. Much of these structures is unneccessary 
 * and impossible (OS thread/mutexes) to persist. Instead save only whats useful to return to this
 * state on restore. This includes recording which threads have acquired what monitors how many times.
 * 
 * @return - false on error (likely memory allocation issue)
 */
bool
VMSnapshotImpl::saveThreadsAndMonitors(void)
{
	bool success = true;
	J9InternalVMFunctions* vmFuncs = _vm->internalVMFunctions;
	J9VMThread* threadCursor = _vm->mainThread;
	J9AcquiredMonitor* acquiredMonitorCursor = _acquiredMonitors;

	do {
		IDATA infoLen = 0;

		/* Save VM JCL monitors. There may be other vm monitors that need to be saved... */
		if (!saveAcquiredMonitor(threadCursor, &acquiredMonitorCursor, _vm->unsafeMemoryTrackingMutex, FALSE, FIXUPREFVM_UNSAFE_MEMORY_TRACKING_MUTEX)) {
			success = false;
			goto done;
		}
		if (!saveAcquiredMonitor(threadCursor, &acquiredMonitorCursor, _vm->verboseStateMutex, FALSE, FIXUPREFVM_VERBOSE_STATE_MUTEX)) {
			success = false;
			goto done;
		}
		if (!saveAcquiredMonitor(threadCursor, &acquiredMonitorCursor, _vm->jclCacheMutex, FALSE, FIXUPREFVM_JCL_CACHE_MUTEX)) {
			success = false;
			goto done;
		}
		if (!saveAcquiredMonitor(threadCursor, &acquiredMonitorCursor, _vm->constantDynamicMutex, FALSE, FIXUPREFVM_CONSTANT_DYNAMIC_MUTEX)) { /* Java 11 */
			success = false;
			goto done;
		}

		/* fixup information for thread waiting to acquire a monitor. */
		saveSystemMonitorWaiting(threadCursor);

		/* Save thread's object monitors that are inflated and acquired. */
		infoLen = vmFuncs->getOwnedObjectMonitors(threadCursor, threadCursor, NULL, 0);
		if (-1 == infoLen) {
			success = false;
			goto done;
		} else if (infoLen > 0) {
			PORT_ACCESS_FROM_PORT(_portLibrary);

			J9ObjectMonitorInfo *info = (J9ObjectMonitorInfo*) j9mem_allocate_memory(infoLen * sizeof(J9ObjectMonitorInfo), J9MEM_CATEGORY_CLASSES);
			if (NULL == info) {
				success = false;
				goto done;
			}

			if (infoLen != vmFuncs->getOwnedObjectMonitors(threadCursor, threadCursor, info, infoLen)) {
				j9mem_free_memory(info);
				success = false;
				goto done;
			}

			/* process object monitors */
			for (IDATA i = 0; i < infoLen; i++) {
				J9ObjectMonitor *objectMonitor = monitorTablePeek(_vm, info[i].object);

				/* objectMonitor will be NULL if deflated. */
				if (NULL != objectMonitor) {
					if (!saveAcquiredMonitor(threadCursor, &acquiredMonitorCursor, objectMonitor->monitor, TRUE, (UDATA)info[i].object)) {
						success = false;
						j9mem_free_memory(info);
						goto done;
					}
				}
			}

			j9mem_free_memory(info);	
		}

		/* Catch-all for threads that should not be restored and threads that currently
		 * can't be restored because they are in a native frame.
		 *
		 * note: not a complete list
		 */
		UDATA notSafeToRestorePublicFlags = J9_PUBLIC_FLAGS_HALTED_AT_SAFE_POINT |
											J9_PUBLIC_FLAGS_HALT_THREAD_INSPECTION |
											J9_PUBLIC_FLAGS_THREAD_PARKED |
											J9_PUBLIC_FLAGS_THREAD_TIMED;


		/* Record application category type since omrthreads are not persisted. 
		 * Daemon thread type is recorded in Java_java_lang_Thread_startImpl */
		if (J9THREAD_CATEGORY_APPLICATION_THREAD == omrthread_get_category(threadCursor->osThread)
			&& (NULL != threadCursor->threadObject) /* only persist java threads */
			&& (0 == threadCursor->inNative) /* can't restore if the thread is in a native frame */
			&& (J9_ARE_NO_BITS_SET(notSafeToRestorePublicFlags, threadCursor->publicFlags))
		) {
			threadCursor->privateFlags2 |= J9_PRIVATE_FLAGS2_SNAPSHOT_APPLICATION_THREAD;

			/* TODO hack in ELS, will need a real solution in the future. 
			 * Creating a copy of this large structure will impact footprint.
			 */
			memcpy(&(threadCursor->restoreEls), threadCursor->entryLocalStorage, sizeof(J9VMEntryLocalStorage));

			/* Force refresh application thread's TLH. */
			success = threadCursor->javaVM->memoryManagerFunctions->J9RefreshTLH(threadCursor);
			if (false == success) {
				printf("ERROR: Snapshot TLH refresh was unsuccessful\n");
				goto done;
			}
		} else {
			Trc_VM_Snapshot_NotPersistingThread(threadCursor);
		}

		/* save monitors from next thread */
		threadCursor = threadCursor->linkNext;
	} while (threadCursor != _vm->mainThread);

	Trc_VM_Snapshot_TotalSavedMonitors(_acquiredMonitorHeader->numOfAcquiredMonitors);

done:
	return success;
}

bool
VMSnapshotImpl::preWriteToImage(J9VMThread *currentThread, VMIntermediateSnapshotState *intermediateSnapshotState)
{
	bool rc = true;

	/* At snapshot time the JVM detaches from omrVM (this is to simplify the restoration process),
	 * after writing the image it re-attaches the VM to omrVM. This has a side effect of changing
	 * the vmThreadKey. So save vmthreadkey so that after snapshot it can be overwritten with the original
	 */
	intermediateSnapshotState->omrVM = _vm->omrVM;
	intermediateSnapshotState->omrRuntime = _vm->omrRuntime;

	/* persist bootstrap classpath in image */
	VMSNAPSHOTIMPLPORT_ACCESS_FROM_JAVAVM(_vm);
	UDATA pathLen = strlen((char *)_vm->bootstrapClassPath);
	char *tempPath = (char *) vmsnapshot_allocate_memory(pathLen + 1, J9MEM_CATEGORY_VM_JCL);
	strncpy(tempPath, (char *) _vm->bootstrapClassPath, pathLen + 1);
	_vm->bootstrapClassPath = (U_8 *) tempPath;

	VM_OutOfLineINL_Helpers::restoreSpecialStackFrameLeavingArgs(currentThread, currentThread->arg0EA);
	currentThread->restoreThreadBP = VM_OutOfLineINL_Helpers::buildInternalNativeStackFrame(currentThread, _triggerMethod);

	detachVMThreadFromOMR(_vm, currentThread);
	_vm->omrVM = NULL;
	_vm->omrRuntime = NULL;

	fixupClasses(currentThread);

	_vm->extendedRuntimeFlags2 &= ~J9_EXTENDED_RUNTIME2_RAMSTATE_SNAPSHOT_RUN;

	return rc;
}

bool
VMSnapshotImpl::postWriteToImage(J9VMThread *currentThread, VMIntermediateSnapshotState *intermediateSnapshotState)
{
	bool rc = true;

	_vm->extendedRuntimeFlags2 |= J9_EXTENDED_RUNTIME2_RAMSTATE_SNAPSHOT_RUN;

	_vm->omrVM = intermediateSnapshotState->omrVM;
	_vm->omrRuntime = intermediateSnapshotState->omrRuntime;

	if (JNI_OK != attachVMThreadToOMR(_vm, currentThread, omrthread_self())) {
		rc = false;
		goto done;
	}

	VM_OutOfLineINL_Helpers::restoreInternalNativeStackFrame(currentThread);
	currentThread->arg0EA = VM_OutOfLineINL_Helpers::buildSpecialStackFrame(currentThread, J9SF_FRAME_TYPE_GENERIC_SPECIAL, J9_SSF_METHOD_ENTRY, false);

done:
	return rc;
}

/**
 * Will be removed once the J9JavaVM struct is fully persisted
 */
bool
VMSnapshotImpl::restoreJ9JavaVMStructures(void)
{
	bool success = true;
	fixupVMStructures();

	fixupClassLoaders();
    if (!restoreJITConfig()) {
		success = false;
		goto done;
	}

	if (omrthread_monitor_init_with_name(&_vm->classMemorySegments->segmentMutex, 0, "VM class mem segment list")) {
		success = false;
	}

	if (omrthread_monitor_init_with_name(&_vm->memorySegments->segmentMutex, 0, "VM mem segment list")) {
		success = false;
	}

done:
	return success;
}

bool
VMSnapshotImpl::enterSnapshotMode(J9VMThread *thread)
{
	return 0 == _vm->memoryManagerFunctions->j9gc_enter_heap_snapshot_mode(thread);
}

void
VMSnapshotImpl::leaveSnapshotMode(J9VMThread *thread)
{
	_vm->memoryManagerFunctions->j9gc_leave_heap_snapshot_mode(thread);
}

bool
VMSnapshotImpl::registerGCForSnapshot(J9VMThread *thread)
{
	UDATA rc = _vm->memoryManagerFunctions->j9gc_register_for_snapshot(thread);
	return rc == 0;
}

bool
VMSnapshotImpl::writeImageToFile(J9VMThread *currentThread)
{
	OMRPortLibrary *portLibrary = (OMRPortLibrary *) _portLibrary;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	bool rc = true;
	UDATA bytesWritten = 0;
	UDATA currentFileOffset = 0;
	UDATA pageSize = omrvmem_supported_page_sizes()[0];

	/* calculate aligned file offsets of memory regions */
	UDATA offset = sizeof(J9SnapshotHeader) + (NUM_OF_MEMORY_SECTIONS * sizeof(J9MemoryRegion));
	for (int i = 0; i < NUM_OF_MEMORY_SECTIONS; i++) {
		 offset = ROUND_UP_TO_POWEROF2(offset, pageSize);
		_memoryRegions[i].fileOffset = offset;
		offset += _memoryRegions[i].totalSize;
	}

	_snapshotHeader->imageSize = offset;

	intptr_t fileDescriptor = omrfile_open(_ramCache, EsOpenCreate | EsOpenCreateAlways | EsOpenWrite, 0666);
	if (-1 == fileDescriptor) {
		rc = false;
		goto done;
	}

	bytesWritten = omrfile_write(fileDescriptor, (void *)_snapshotHeader, sizeof(J9SnapshotHeader));
	if (sizeof(J9SnapshotHeader) != bytesWritten) {
		rc = false;
		goto done;
	}

	currentFileOffset += bytesWritten;
	bytesWritten = omrfile_write(fileDescriptor, (void *)_acquiredMonitorHeader, sizeof(J9AcquiredMonitorHeader));
	if (sizeof(J9AcquiredMonitorHeader) != bytesWritten) {
		rc = false;
		goto done;
	}

	currentFileOffset += bytesWritten;
	bytesWritten = omrfile_write(fileDescriptor, (void *)_acquiredMonitors, sizeof(J9AcquiredMonitor) * MAX_NUM_ALLOCATED_MONITORS);
	if ((MAX_NUM_ALLOCATED_MONITORS * sizeof(J9AcquiredMonitor)) != bytesWritten) {
		rc = false;
		goto done;
	}

	currentFileOffset += bytesWritten;
	bytesWritten = omrfile_write(fileDescriptor, (void *)_memoryRegions, sizeof(J9MemoryRegion) * NUM_OF_MEMORY_SECTIONS);
	if ((NUM_OF_MEMORY_SECTIONS * sizeof(J9MemoryRegion)) != bytesWritten) {
		rc = false;
		goto done;
	}

	for (int i = 0; i < NUM_OF_MEMORY_SECTIONS; i++) {
		currentFileOffset += bytesWritten;
		/* pad until aligned file offset */
		UDATA padbytes = _memoryRegions[i].fileOffset - currentFileOffset;

		if (omrfile_seek(fileDescriptor, padbytes, EsSeekCur) != (IDATA)_memoryRegions[i].fileOffset) {
			rc = false;
			goto done;
		}

		bytesWritten = omrfile_write(fileDescriptor, (void *)_memoryRegions[i].alignedStartAddr, _memoryRegions[i].totalSize);
		if (_memoryRegions[i].totalSize != bytesWritten) {
			rc = false;
			goto done;
		}

		bytesWritten += padbytes;
	}

done:
	if (-1 == fileDescriptor) {
		omrfile_close(fileDescriptor);
	}

	return rc;
}

void
VMSnapshotImpl::freeJ9JavaVMStructures(void)
{
	/* TODO Temporary function to free VM structures since snapshot is being created at shutdown.
	 * Some structures cannot be freed at their normal position because they need to be kept
	 * around so they can be written into the image. This will be removed in the future.
	 */
	freeHiddenInstanceFieldsList(_vm);
}

bool
VMSnapshotImpl::writeSnapshotImage(J9VMThread *currentThread)
{
	bool rc = true;
	VMIntermediateSnapshotState intermediateSnapshotState = {0};
	OMRPORT_ACCESS_FROM_J9PORT(_portLibrary);

	acquireExclusiveVMAccess(currentThread);

	if (!enterSnapshotMode(currentThread)) {
		omrtty_err_printf("Error: Failed to enter snapshot mode\n");
		rc = false;
		goto done;
	}

	registerGCForSnapshot(currentThread);

	saveJ9JavaVMStructures();

	TRIGGER_J9HOOK_TRIGGER_SNAPSHOT(currentThread->javaVM->hookInterface, currentThread);
	
	if (!saveThreadsAndMonitors()) {
		rc = false;
		goto done;
	}

	if (!preWriteToImage(currentThread, &intermediateSnapshotState)) {
		rc = false;
		goto done;
	}

	if(!writeImageToFile(currentThread)) {
		omrtty_err_printf("Error: Failed to write snapshot to file\n");
		rc = false;
		goto done;
	}

	if (!postWriteToImage(currentThread, &intermediateSnapshotState)) {
		rc = false;
		goto done;
	}

	leaveSnapshotMode(currentThread);

#if defined(DEBUG)
	{
		OMRPORT_ACCESS_FROM_J9PORT(_portLibrary);
		J9MemoryRegion *gcRegion = &_memoryRegions[GC_HEAP];
		omrtty_err_printf("==== GC Region Stats ====\n");
		omrtty_err_printf("heapBase    = %p\n",  (void*) gcRegion->alignedStartAddr);
		omrtty_err_printf("heapMinSize = %zu\n", (size_t)gcRegion->totalSize);
		omrtty_err_printf("heapMaxSize = %zu\n", (size_t)gcRegion->mappableSize);
		omrtty_err_printf("=========================\n");

	}
#endif /* defined(DEBUG) */

done:
	releaseExclusiveVMAccess(currentThread);
	return rc;
}

J9MemoryRegion
VMSnapshotImpl::getGCHeapMemoryRegion() const
{
	return _memoryRegions[GC_HEAP];
}


void
VMSnapshotImpl::setGCHeapMemoryRegion(const J9MemoryRegion *region)
{
	_memoryRegions[region->type] = *region;
}

J9GCSnapshotProperties
VMSnapshotImpl::getGCSnapshotProperties() const
{
	return _snapshotHeader->gcProperties;
}

void
VMSnapshotImpl::setGCSnapshotProperties(const J9GCSnapshotProperties *properties)
{
	_snapshotHeader->gcProperties = *properties;
}

intptr_t
VMSnapshotImpl::getSnapshotFD() const
{
	return _snapshotFD;
}

bool
VMSnapshotImpl::postPortLibInitstage(bool isSnapShotRun)
{
	bool rc = true;

	if (!initializeInvalidITable()) {
		rc = false;
		goto done;
	}
done:
	return rc;
}

VMSnapshotImplPortLibrary *
setupVMSnapshotImplPortLibrary(VMSnapshotImpl *vmSnapshotImpl, J9PortLibrary *portLibrary, BOOLEAN snapshotRun)
{
	VMSnapshotImplPortLibrary *vmSnapshotImplPortLibrary = NULL;
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);

	if (snapshotRun) {
		vmSnapshotImplPortLibrary = (VMSnapshotImplPortLibrary*)privateOmrPortLibrary->heap_allocate(privateOmrPortLibrary, vmSnapshotImpl->getSubAllocatorHeap(), sizeof(VMSnapshotImplPortLibrary));
	} else {
		vmSnapshotImplPortLibrary = vmSnapshotImpl->getSnapshotHeader()->savedJavaVMStructs.vmSnapshotImplPortLibrary;
	}

	if (NULL != vmSnapshotImplPortLibrary)  {
		memcpy(&(vmSnapshotImplPortLibrary->portLibrary), privateOmrPortLibrary, sizeof(OMRPortLibrary));
		vmSnapshotImplPortLibrary->portLibrary.mem_allocate_memory = image_mem_allocate_memory;
		vmSnapshotImplPortLibrary->portLibrary.mem_free_memory = image_mem_free_memory;
		vmSnapshotImplPortLibrary->portLibrary.mem_allocate_memory32 = image_mem_allocate_memory32;
		vmSnapshotImplPortLibrary->portLibrary.mem_free_memory32 = image_mem_free_memory32;
		vmSnapshotImplPortLibrary->vmSnapshotImpl = vmSnapshotImpl;

		vmSnapshotImpl->setImagePortLib(vmSnapshotImplPortLibrary);
	}

	return vmSnapshotImplPortLibrary;
}

void
resetVMThreadState(J9JavaVM *vm, J9VMThread *restoreThread)
{
	restoreThread->profilingBufferEnd = NULL;
	restoreThread->profilingBufferCursor = NULL;
	restoreThread->jitExceptionHandlerCache = NULL;
	restoreThread->jitArtifactSearchCache = NULL;
	restoreThread->aotVMwithThreadInfo = NULL;
	restoreThread->jitVMwithThreadInfo = NULL;
	restoreThread->jitPrivateData = NULL;
	restoreThread->publicFlagsMutex = NULL;
	restoreThread->jniArrayCache = NULL;
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
	restoreThread->jitCountDelta = 2;
	restoreThread->maxProfilingCount = (3000 * 2) + 1;
#if defined(J9VM_ENV_SHARED_LIBS_USE_GLOBAL_TABLE)
	restoreThread->jitTOC = vm->jitTOC;
#endif
#endif
	restoreThread->eventFlags = vm->globalEventFlags;
	restoreThread->functions = (JNINativeInterface_*) vm->jniFunctionTable;
}

/**
 * Restore acquired object-monitor if it matches the fixup information in monitor.
 * 
 * @param monitor fixup information for acquired monitor. This will be incremented to the next data structure if object-monitor is successfully restored.
 * @param monitorCount number of monitors left to fix up. Should be decremented if monitor is successfully restored.
 * @param object object-monitor to restore with monitor information
 * 
 * @return true if successful, else false
 */
bool
VMSnapshotImpl::restoreAcquiredObjectMonitor(J9AcquiredMonitor** monitor, UDATA* monitorCount, j9object_t object)
{
	bool rc = true;
	if ((*monitorCount > 0) && ((*monitor)->isObjectMonitor) && ((*monitor)->fixupReference == (UDATA)object)) {
		J9VMThread* monitorOwnerThread = (J9VMThread*)(*monitor)->ownerVmThreadAddress;

		omrthread_t omrThread = monitorOwnerThread->osThread;

		/* re-force inflation so the omrthread_monitor_t can be fixed up */
		j9objectmonitor_t *lockEA = J9OBJECT_MONITOR_EA(monitorOwnerThread, object);
		j9objectmonitor_t lock = J9_LOAD_LOCKWORD(monitorOwnerThread, lockEA);

		J9ObjectMonitor *objectMonitor = objectMonitorInflate(monitorOwnerThread, object, lock);
		if (NULL == objectMonitor) {
			rc = false;
			goto done;
		}

		omrthread_monitor_t omrMonitor = objectMonitor->monitor;

		/* The omrthread_monitor_t:count may be wrong since objectMonitorInflate sets it to J9_FLATLOCK_COUNT(lock).
		 * J9_FLATLOCK_COUNT is only valid if the lock is flat and since this monitor was recorded
		 * to have been in an inflated state at the snapshot point this count is innacurate.
		 * 
		 * The correct count was persisted as J9AcquiredMonitor:ownerCount. Since inflated monitors
		 * are only restored if they are acquired at the snapshot point ownerCount must be >= 1.
		 * It is initially set to 1 here because the monitor has already been acquired once during
		 * the call to objectMonitorInflate.
		 * 
		 * If ownerCount is more than one the owner thread should acquire the monitor multiple
		 * times. This state is restored during the for loop and will update the count to be correct
		 */
		((J9ThreadAbstractMonitor*)omrMonitor)->count = 1;

		for (U_32 i = 1; i < (*monitor)->ownerCount; i++) {
			if (0 != omrthread_monitor_try_enter_using_threadId(omrMonitor, omrThread)) {
				rc = false;
				goto done;
			}
		}

		Assert_VM_true((*monitor)->ownerCount == omrthread_monitor_get_acquired_count(omrMonitor));

		*monitor = *monitor + 1;
		*monitorCount = *monitorCount - 1;
	}

done:
	return rc;
}

/**
 * Restore acquired system monitor if it matches the fixup information in monitor.
 * 
 * @param monitor fixup information for acquired monitor. This will be incremented to the next data structure if monitor is successfully restored.
 * @param monitorCount number of monitors left to fix up. Should be decremented if monitor is successfully restored.
 * @param omrMonitorReference
 * @param fixupReference
 * 
 * @return true if successful, else false
 */
bool
VMSnapshotImpl::restoreSystemMonitor(J9AcquiredMonitor **monitor, UDATA *monitorCount, omrthread_monitor_t omrMonitorReference, UDATA fixupReference)
{
	bool rc = true;

	if ((*monitorCount > 0) && (!(*monitor)->isObjectMonitor) && (fixupReference == (*monitor)->fixupReference)) {
		omrthread_t omrThread = ((J9VMThread*)(*monitor)->ownerVmThreadAddress)->osThread;

		for (U_32 i = 0; i < (*monitor)->ownerCount; i++) {
			if (0 != omrthread_monitor_try_enter_using_threadId(omrMonitorReference, omrThread)) {
				rc = false;
				goto done;
			}
		}

		*monitor = *monitor + 1;
		*monitorCount = *monitorCount - 1;
	}

done:
	return rc;
}

/**
 * Restore the state of acquired monitors.
 * 
 * @return true if successful, else false
 */
bool
VMSnapshotImpl::restoreMonitors(void)
{
	bool rc = true;

	if (_acquiredMonitorHeader->numOfAcquiredMonitors > 0) {
		J9AcquiredMonitor* monitorData = _acquiredMonitors;
		UDATA monitorCount = _acquiredMonitorHeader->numOfAcquiredMonitors;
		J9InternalVMFunctions* vmFuncs = _vm->internalVMFunctions;
		IDATA infoLen = 0;
		J9VMThread* threadCursor = NULL;

		/* Process vm monitors */
		rc = restoreSystemMonitor(&monitorData, &monitorCount, _vm->unsafeMemoryTrackingMutex, FIXUPREFVM_UNSAFE_MEMORY_TRACKING_MUTEX);
		if (false == rc) {
			goto done;
		}

		rc = restoreSystemMonitor(&monitorData, &monitorCount, _vm->verboseStateMutex, FIXUPREFVM_VERBOSE_STATE_MUTEX);
		if (false == rc) {
			goto done;
		}

		rc = restoreSystemMonitor(&monitorData,  &monitorCount, _vm->jclCacheMutex, FIXUPREFVM_JCL_CACHE_MUTEX);
		if (false == rc) {
			goto done;
		}

		rc = restoreSystemMonitor(&monitorData, &monitorCount, _vm->constantDynamicMutex, FIXUPREFVM_CONSTANT_DYNAMIC_MUTEX); /* Java 11 */
		if (false == rc) {
			goto done;
		}

		/* Restore object monitors. This should be done one thread at a time in the same order in which they were saved. */
		threadCursor = _vm->mainThread;
		do {
			infoLen = vmFuncs->getOwnedObjectMonitors(threadCursor, threadCursor, NULL, 0);
			if (-1 == infoLen) {
				rc = false;
				goto done;
			} else if (infoLen > 0) {
				PORT_ACCESS_FROM_PORT(_portLibrary);

				J9ObjectMonitorInfo *info = (J9ObjectMonitorInfo*) j9mem_allocate_memory(infoLen * sizeof(J9ObjectMonitorInfo), J9MEM_CATEGORY_CLASSES);
				if (NULL == info) {
					goto done;
				}

				if (infoLen != vmFuncs->getOwnedObjectMonitors(threadCursor, threadCursor, info, infoLen)) {
					j9mem_free_memory(info);
					rc = false;
					goto done;
				}

				/* process object monitors - should be in the same order as during snapshot */
				for (IDATA i = 0; i < infoLen; i++) {
					/* objects should be in the same order as on restore. */
					rc = restoreAcquiredObjectMonitor(&monitorData, &monitorCount, info[i].object);
					if (false == rc) {
						j9mem_free_memory(info);
						goto done;
					}
				}

				j9mem_free_memory(info);

			}

			threadCursor = threadCursor->linkNext;
		} while (threadCursor != _vm->mainThread);

		if (monitorCount > 0) {
			printf("ERROR: somehow not all monitors were restored\n");
			rc = false;
		}
	}

done:
	return rc;
}

/**
 * If vmThread should be waiting on a system monitor
 * attempt to acquire it here.
 * @param vmThread
 */
static void
restoreSystemMonitorWaiting(J9VMThread* vmThread) {
	UDATA fixupId = vmThread->waitingMonitorFixupId;
	if (0 != fixupId) {
		J9JavaVM* vm = vmThread->javaVM;
		switch(fixupId) {
		case FIXUPREFVM_UNSAFE_MEMORY_TRACKING_MUTEX:
			omrthread_monitor_enter(vm->unsafeMemoryTrackingMutex);
			break;
		case FIXUPREFVM_VERBOSE_STATE_MUTEX:
			omrthread_monitor_enter(vm->verboseStateMutex);
			break;
		case FIXUPREFVM_JCL_CACHE_MUTEX:
			omrthread_monitor_enter(vm->jclCacheMutex);
			break;
		case FIXUPREFVM_CONSTANT_DYNAMIC_MUTEX:
			omrthread_monitor_enter(vm->constantDynamicMutex);
			break;
		}
	}
}

/**
 * Process resumes interpreter for arg thread
 * 
 * @param thread associated with running OMR thread.
 * @return 0 for success
 */
static int J9THREAD_PROC
procRestoreThreadState(void* arg)
{
	J9VMThread* vmThread = (J9VMThread*)arg;

	/* Restore special frame. Otherwise last bytecode will be run. */
	if ((UDATA)(vmThread->pc) <= J9SF_MAX_SPECIAL_FRAME_TYPE) {
		if (J9_ARE_ALL_BITS_SET((UDATA)(vmThread->pc), J9SF_FRAME_TYPE_NATIVE_METHOD)) {
			VM_OutOfLineINL_Helpers::restoreInternalNativeStackFrame(vmThread);
		} else {
			VM_OutOfLineINL_Helpers::restoreSpecialStackFrameLeavingArgs(vmThread, vmThread->arg0EA);
		}
	}

	/* if thread is waiting to acquire a vm monitor stat waiting here. */
	restoreSystemMonitorWaiting(vmThread);

	restoreThreadState(vmThread);

	/* Destroy Java thread similarly to what happens in Java_java_lang_Thread_startImpl */
	threadCleanup(vmThread, FALSE);
	return 0;
}

extern "C" BOOLEAN
initRestoreThreads(void *vmSnapshotImpl, J9JavaVM *vm, omrthread_t mainOSThread)
{
	bool rc = true;
	J9MemoryManagerFunctions* gcFuncs = vm->memoryManagerFunctions;
	/* Restore main thread last so it becomes the "currentThread" */
	J9VMThread *restoreThread = vm->mainThread->linkNext;

	do {
		omrthread_t restoreOSThread = NULL;
		char *threadName = NULL;
		OMR_VMThread *omrVMThread = NULL;

		if (vm->mainThread == restoreThread) {
			restoreOSThread = mainOSThread;
			restoreThread->privateFlags2 |= J9_PRIVATE_FLAGS2_RESTORE_MAINTHREAD;
		} else if (J9_ARE_ALL_BITS_SET(restoreThread->privateFlags2, J9_PRIVATE_FLAGS2_SNAPSHOT_APPLICATION_THREAD)) {
			/* non-main application threads should be set up to run procRestoreThreadState when resumed. */
			if (createThreadWithCategory(&restoreOSThread, 0, J9THREAD_PRIORITY_NORMAL, TRUE, procRestoreThreadState, (void*)restoreThread, J9THREAD_CATEGORY_APPLICATION_THREAD) != J9THREAD_SUCCESS) {
				rc = false;
				goto done;
			}

			/* ELS fix needs to be done before fixing up object monitors */
			restoreThread->entryLocalStorage = &(restoreThread->restoreEls);
			restoreThread->entryLocalStorage->oldEntryLocalStorage = NULL;
		} else {
			omrthread_attr_t attr = NULL;
			UDATA category = J9THREAD_CATEGORY_SYSTEM_THREAD;
			if (J9_ARE_ALL_BITS_SET(restoreThread->privateFlags, J9_PRIVATE_FLAGS_DAEMON_THREAD)) {
				category = J9THREAD_CATEGORY_RESOURCE_MONITOR_THREAD;
			}

			if (omrthread_attr_init(&attr) != J9THREAD_SUCCESS) {
				rc = false;
				goto done;
			}
			if (omrthread_attr_set_category(&attr, category) != J9THREAD_SUCCESS) {
				rc = false;
				goto done;
			}

			if (omrthread_attach_ex(&restoreOSThread, &attr) != J9THREAD_SUCCESS) {
				rc = false;
				goto done;
			}
		}

		restoreThread->osThread = restoreOSThread;

		resetVMThreadState(vm, restoreThread);

		omrVMThread = (OMR_VMThread*)(((UDATA)restoreThread) + J9_VMTHREAD_SEGREGATED_ALLOCATION_CACHE_OFFSET + vm->segregatedAllocationCacheSize);
		memset(omrVMThread, 0, sizeof(OMR_VMThread));

		omrthread_monitor_init_with_name(&restoreThread->publicFlagsMutex, J9THREAD_MONITOR_JLM_TIME_STAMP_INVALIDATOR, "Thread public flags mutex");
		if (restoreThread->publicFlagsMutex == NULL) {
			rc = false;
			goto done;
		}

		initOMRVMThread(vm, restoreThread);

		if (JNI_OK != attachVMThreadToOMR(vm, restoreThread, restoreOSThread)) {
			rc = false;
			goto done;
		}

		if (0 != gcFuncs->initializeMutatorModelJava(restoreThread)) {
			rc = false;
			goto done;
		}

		if (J9_ARE_ALL_BITS_SET(restoreThread->privateFlags2, J9_PRIVATE_FLAGS2_SNAPSHOT_APPLICATION_THREAD)) {
		#ifdef J9VM_IVE_RAW_BUILD /* J9VM_IVE_RAW_BUILD is not enabled by default */
			{
				j9object_t unicodeChars = J9VMJAVALANGTHREAD_NAME(vm->mainThread, restoreThread->threadObject);
				if (NULL != unicodeChars) {
					threadName = copyStringToUTF8WithMemAlloc(vm->mainThread, unicodeChars, J9_STR_NULL_TERMINATE_RESULT, "", 0, NULL, 0, NULL);
				}
			}
		#else /* J9VM_IVE_RAW_BUILD */
			threadName = getVMThreadNameFromString(vm->mainThread, J9VMJAVALANGTHREAD_NAME(vm->mainThread, restoreThread->threadObject));
		#endif /* J9VM_IVE_RAW_BUILD */
			if (threadName == NULL) {
				Trc_VM_startJavaThread_failedVMThreadAlloc(restoreThread);
				omrthread_cancel(restoreOSThread);
				rc = false;
				goto done;
			}

			setOMRVMThreadNameWithFlag(vm->mainThread->omrVMThread, restoreThread->omrVMThread, threadName, TRUE);
		#if !defined(LINUX)
			/* on Linux this is done by the new thread when it starts running */
			omrthread_set_name(restoreOSThread, threadName);
		#endif

			/* Fixup objectMonitorLookupCache - omrthread_monitor_t mutex in J9ObjectMonitor will not have been persisted */
			for (UDATA i = 0; i < J9VM_OBJECT_MONITOR_CACHE_SIZE; i++) {
				j9objectmonitor_t cacheEntry = restoreThread->objectMonitorLookupCache[i];
				if (0 != cacheEntry) {
					J9ObjectMonitor* objectMonitor = (J9ObjectMonitor*) ((UDATA) cacheEntry);
					printf("om is: %p\n", objectMonitor);
					if (J9_LOCK_IS_INFLATED(objectMonitor->alternateLockword)) {
						printf("om is inflated\n");

					}
				}
			}
		}

		restoreThread = restoreThread->linkNext;
	} while (vm->mainThread->linkNext != restoreThread);

	/* once threads are restored fixup locks */
	rc = ((VMSnapshotImpl *)vmSnapshotImpl)->restoreMonitors();

done:
	return rc;
}

extern "C" BOOLEAN
setupSnapshotMethodTrigger(void *vmSnapshotImpl)
{
	return (BOOLEAN)((VMSnapshotImpl *)vmSnapshotImpl)->setupMethodTrigger();
}

extern "C" BOOLEAN
interceptMainAndRestoreSnapshotState(J9VMThread *currentThread, jmethodID methodID)
{
	J9JavaVM *vm = currentThread->javaVM;
	BOOLEAN rc = FALSE;

	if (IS_RESTORE_RUN(vm) && J9_ARE_NO_BITS_SET(vm->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_SNAPSHOT_STATE_IS_RESTORED)) {
		J9JNIMethodID *jniMethodRef = (J9JNIMethodID *) methodID;
		J9UTF8 *romMethodName = J9ROMMETHOD_NAME(J9_ROM_METHOD_FROM_RAM_METHOD(jniMethodRef->method));

		if (J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(romMethodName), J9UTF8_LENGTH(romMethodName), "main")) {
			/* Run Snapshot API restore hooks before resuming threads */
			J9NameAndSignature nas = {0};
			nas.name = (J9UTF8 *)&runPostRestoreHooks_name;
			nas.signature = (J9UTF8 *)&runPostRestoreHooks_sig;

			vm->extendedRuntimeFlags2 |= J9_EXTENDED_RUNTIME2_SNAPSHOT_STATE_IS_RESTORED;
			VM_OutOfLineINL_Helpers::recordJNIReturn(currentThread, currentThread->restoreThreadBP);
			VM_OutOfLineINL_Helpers::restoreSpecialStackFrameLeavingArgs(currentThread, currentThread->restoreThreadBP);

			runStaticMethod(currentThread, (U_8 *)"com/ibm/oti/vm/SnapshotControlAPI", &nas, 0, NULL);

			if (VM_VMHelpers::exceptionPending(currentThread)) {
				UDATA *bp = VM_OutOfLineINL_Helpers::buildSpecialStackFrame(currentThread, J9SF_FRAME_TYPE_GENERIC_SPECIAL, currentThread->jitStackFrameFlags, false);
				setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
				VM_OutOfLineINL_Helpers::restoreSpecialStackFrameLeavingArgs(currentThread, bp);
			} else {
				/* acquire exclusive while resuming application threads so thread linkedList doesn't change. */
				acquireExclusiveVMAccess(currentThread);
				/* If restore hook run was successful resume all suspended application threads. Non-main threads
				 * have been setup to call restoreThreadState when resumed. */
				J9VMThread* threadCursor = currentThread->linkNext;
				while (currentThread != threadCursor) {
					if (J9_ARE_ALL_BITS_SET(threadCursor->privateFlags2, J9_PRIVATE_FLAGS2_SNAPSHOT_APPLICATION_THREAD)) {
						omrthread_resume(threadCursor->osThread);
					}
					threadCursor = threadCursor->linkNext;
				}
				releaseExclusiveVMAccess(currentThread);
				restoreThreadState(currentThread);
			}

			rc = TRUE;
		}
	}

	return rc;
}

extern "C" void *
initializeVMSnapshotImpl(J9PortLibrary *portLibrary, BOOLEAN isSnapShotRun, const char* ramCache, const char* trigger)
{
	VMSnapshotImplPortLibrary *vmSnapshotImplPortLibrary = NULL;
	VMSnapshotImpl *vmSnapshotImpl = VMSnapshotImpl::createInstance(portLibrary, ramCache, trigger);
	if (NULL == vmSnapshotImpl) {
		goto _error;
	}

	/* Call the appropriate setup api based on the mode
	 * we're running in
	 */
	if (isSnapShotRun) {
		if (!vmSnapshotImpl->setupColdRun()) {
			goto _error;
		}
	} else {
		if (!vmSnapshotImpl->setupWarmRun()) {
			goto _error;
		}
	}

	vmSnapshotImplPortLibrary = setupVMSnapshotImplPortLibrary(vmSnapshotImpl, portLibrary, isSnapShotRun);
	if (NULL == vmSnapshotImplPortLibrary) {
		goto _error;
	}

	if(!vmSnapshotImpl->postPortLibInitstage(isSnapShotRun)) {
		goto _error;
	}

	return vmSnapshotImpl;

_error:
	shutdownVMSnapshotImpl(vmSnapshotImpl, portLibrary);
	return NULL;
}

extern "C" void
setupVMSnapshotImpl(void *vmSnapshotImpl, J9JavaVM* vm)
{
	VMSnapshotImpl *impl = static_cast<VMSnapshotImpl *>(vmSnapshotImpl);
	J9JavaVM* implVM = impl->getJ9JavaVM();
	if (implVM != NULL) {
		if (implVM != vm) {
			/* Something seriously wrong here! */
			printf("**** In a Restore run and was passed a different vm pointer.\n");
			printf("**** Expected VMSnapshotImpl->_vm(%p) was (%p)\n", implVM, vm);
			abort();
		}
		/* No action required */
	} else {
		/* Snapshot run - initial set of VM into VMSnapshotImpl*/
		impl->setJ9JavaVM(vm);
	}
	vm->vmSnapshotImplPortLibrary = impl->getVMSnapshotImplPortLibrary();
}

extern "C" J9JavaVM *
getJ9JavaVMFromVMSnapshotImpl(void *vmSnapshotImpl)
{
	return static_cast<VMSnapshotImpl *>(vmSnapshotImpl)->getJ9JavaVM();
}

extern "C" VMSnapshotImplPortLibrary *
getPortLibraryFromVMSnapshotImpl(void *vmSnapshotImpl)
{
	return ((VMSnapshotImpl *)vmSnapshotImpl)->getVMSnapshotImplPortLibrary();
}

extern "C" void
initializeImageClassLoaderObject(J9JavaVM *javaVM, J9ClassLoader *classLoader, j9object_t classLoaderObject)
{
	J9VMThread *vmThread = currentVMThread(javaVM);

	omrthread_monitor_enter(javaVM->classLoaderBlocksMutex);

	J9CLASSLOADER_SET_CLASSLOADEROBJECT(vmThread, classLoader, classLoaderObject);

	issueWriteBarrier();
	J9VMJAVALANGCLASSLOADER_SET_VMREF(vmThread, classLoaderObject, classLoader);

	omrthread_monitor_exit(javaVM->classLoaderBlocksMutex);
}

extern "C" void
initializeImageJ9Class(J9JavaVM *javaVM, J9Class *clazz)
{
	VMSnapshotImpl *vmSnapshotImpl = (VMSnapshotImpl *)javaVM->vmSnapshotImplPortLibrary->vmSnapshotImpl;
	Assert_VM_notNull(vmSnapshotImpl);

	vmSnapshotImpl->fixupJITVtable(clazz);
}

extern "C" void
shutdownVMSnapshotImpl(void *vmSnapshotImpl, J9PortLibrary *portLib)
{
	if (NULL != vmSnapshotImpl) {
		PORT_ACCESS_FROM_PORT(portLib);
		((VMSnapshotImpl *)vmSnapshotImpl)->~VMSnapshotImpl();
		j9mem_free_memory(vmSnapshotImpl);
	}
}

void *
image_mem_allocate_memory32(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount, const char *callSite, uint32_t category)
{
	VMSnapshotImpl *vmSnapshotImpl = (VMSnapshotImpl *)((VMSnapshotImplPortLibrary *)portLibrary)->vmSnapshotImpl;

	Assert_VM_notNull(vmSnapshotImpl);

	void *pointer = vmSnapshotImpl->subAllocateMemory(byteAmount, true);
	return pointer;
}

void
image_mem_free_memory32(struct OMRPortLibrary *portLibrary, void *memoryPointer)
{
	VMSnapshotImpl *vmSnapshotImpl = (VMSnapshotImpl *)((VMSnapshotImplPortLibrary *)portLibrary)->vmSnapshotImpl;
	Assert_VM_notNull(vmSnapshotImpl);

	vmSnapshotImpl->freeSubAllocatedMemory(memoryPointer, true);
}

void *
image_mem_allocate_memory(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount, const char *callSite, uint32_t category)
{
	VMSnapshotImpl *vmSnapshotImpl = (VMSnapshotImpl *)((VMSnapshotImplPortLibrary *)portLibrary)->vmSnapshotImpl;

	Assert_VM_notNull(vmSnapshotImpl);

	void *pointer = vmSnapshotImpl->subAllocateMemory(byteAmount, false);
	return pointer;
}

void
image_mem_free_memory(struct OMRPortLibrary *portLibrary, void *memoryPointer)
{
	VMSnapshotImpl *vmSnapshotImpl = (VMSnapshotImpl *)((VMSnapshotImplPortLibrary *)portLibrary)->vmSnapshotImpl;
	Assert_VM_notNull(vmSnapshotImpl);

	vmSnapshotImpl->freeSubAllocatedMemory(memoryPointer, false);
}

extern "C" void
getGCHeapMemoryRegion(J9JavaVM *vm, J9MemoryRegion *result)
{
	void *impl = vm->vmSnapshotImplPortLibrary->vmSnapshotImpl;
	*result = ((const VMSnapshotImpl *)impl)->getGCHeapMemoryRegion();
}

extern "C" void
setGCHeapMemoryRegion(J9JavaVM *vm, const J9MemoryRegion *region)
{
	void *impl = vm->vmSnapshotImplPortLibrary->vmSnapshotImpl;
	((VMSnapshotImpl *)impl)->setGCHeapMemoryRegion(region);
}

extern "C" void
getGCSnapshotProperties(J9JavaVM *vm, J9GCSnapshotProperties *result)
{
	void *impl = vm->vmSnapshotImplPortLibrary->vmSnapshotImpl;
	*result = ((const VMSnapshotImpl *)impl)->getGCSnapshotProperties();
}

extern "C" void
setGCSnapshotProperties(J9JavaVM *vm, const J9GCSnapshotProperties *properties)
{
	void *impl = vm->vmSnapshotImplPortLibrary->vmSnapshotImpl;
	((VMSnapshotImpl *)impl)->setGCSnapshotProperties(properties);
}

extern "C" intptr_t
getSnapshotFD(J9JavaVM *vm)
{
	void *impl = vm->vmSnapshotImplPortLibrary->vmSnapshotImpl;
	return ((const VMSnapshotImpl *)impl)->getSnapshotFD();
}

#endif /* defined(J9VM_OPT_SNAPSHOTS) */
