/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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

#include "j9.h"
#include "j9cfg.h"
#include "mmhook.h"
#include "j9port.h"
#include "modronopt.h"
#include "Tgc.hpp"
#include "HeapIteratorAPI.h"

#include "GCExtensions.hpp"
#include "TgcExtensions.hpp"

static jvmtiIterationControl dump_heapIteratorCallback(J9JavaVM* vm, J9MM_IterateHeapDescriptor* heapDesc, void* userData);
static jvmtiIterationControl dump_spaceIteratorCallback(J9JavaVM* vm, J9MM_IterateSpaceDescriptor* spaceDesc, void* userData);
static jvmtiIterationControl dump_regionIteratorCallback(J9JavaVM* vm, J9MM_IterateRegionDescriptor* regionDesc, void* userData);
static jvmtiIterationControl dump_objectIteratorCallback(J9JavaVM* vm, J9MM_IterateObjectDescriptor* objectDesc, void *userData);

typedef struct DumpObjectsIteratorCallbackUserData {
	bool previousObjectWasFreeSpace; /* was the previous object free space or a live object? */
	UDATA gcCount; /* GC identifier */
} DumpObjectsIteratorCallbackUserData;


/**
 * @todo Provide function documentation
 */
static void
tgcHookGlobalGcSweepEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_SweepEndEvent* event = (MM_SweepEndEvent*)eventData;
	J9VMThread *vmThread = static_cast<J9VMThread *>(event->currentThread->_language_vmthread);
	J9JavaVM* javaVM = vmThread->javaVM;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(event->currentThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);

	uintptr_t gcCount =  extensions->getUniqueGCCycleCount();

	tgcExtensions->printf("<GC(%zu) Dumping Middleware Heap free blocks\n", gcCount);

	DumpObjectsIteratorCallbackUserData iteratorData;
	iteratorData.gcCount = gcCount;
	iteratorData.previousObjectWasFreeSpace = false;
	javaVM->memoryManagerFunctions->j9mm_iterate_heaps(javaVM, javaVM->portLibrary, 0, dump_heapIteratorCallback, &iteratorData);
}

static jvmtiIterationControl
dump_heapIteratorCallback(J9JavaVM* vm, J9MM_IterateHeapDescriptor* heapDesc, void* userData)
{
	vm->memoryManagerFunctions->j9mm_iterate_spaces(vm, vm->portLibrary, heapDesc, 0, dump_spaceIteratorCallback, userData);
	return JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
dump_spaceIteratorCallback(J9JavaVM* vm, J9MM_IterateSpaceDescriptor* spaceDesc, void* userData)
{
	vm->memoryManagerFunctions->j9mm_iterate_regions(vm, vm->portLibrary, spaceDesc, 0, dump_regionIteratorCallback, userData);
	return JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
dump_regionIteratorCallback(J9JavaVM* vm, J9MM_IterateRegionDescriptor* regionDesc, void* userData)
{
	DumpObjectsIteratorCallbackUserData* castUserData = (DumpObjectsIteratorCallbackUserData*)userData;
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(vm);
	
	castUserData->previousObjectWasFreeSpace = false;
	
	vm->memoryManagerFunctions->j9mm_iterate_region_objects(vm, vm->portLibrary, regionDesc, j9mm_iterator_flag_include_holes, dump_objectIteratorCallback, castUserData);
	
	/* If the last object in the region was free space, the line was not terminated */
	if (castUserData->previousObjectWasFreeSpace) {
		tgcExtensions->printf(">\n");
	}
	
	return JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
dump_objectIteratorCallback(J9JavaVM* vm, J9MM_IterateObjectDescriptor* objectDesc, void *userData)
{
	DumpObjectsIteratorCallbackUserData* castUserData = (DumpObjectsIteratorCallbackUserData*)userData;
	bool isFreeSpace = false;
	UDATA freeSpaceByteSize = 0;
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(vm);
	
	/* Determine whether or not the object is dead, and get its size. Since the object heap iterator 
	 * doesn't consult the mark map, a "live" object may actually be a dead object in a chunk that wasn't 
	 * large enough to make the free list.
	 */

	if (FALSE == objectDesc->isObject) {
		isFreeSpace = true;
		freeSpaceByteSize = objectDesc->size;
	} else if (!(vm->memoryManagerFunctions->j9gc_ext_is_marked(vm, objectDesc->object))) {
		isFreeSpace = true;
		freeSpaceByteSize = objectDesc->size;
	}
	
	/* Print the next object after free space, and in any case, terminate the line */			
	if (castUserData->previousObjectWasFreeSpace) {
		if (!isFreeSpace) {
			tgcExtensions->printf(" -- x%p ", objectDesc->size);
			tgcPrintClass(vm, J9GC_J9OBJECT_CLAZZ_VM(objectDesc->object, vm));
		}
		tgcExtensions->printf(">\n");	
	}

	if (isFreeSpace) {
		tgcExtensions->printf("<GC(%zu) %p freelen=x%p", castUserData->gcCount, objectDesc->id, freeSpaceByteSize);
	}

	castUserData->previousObjectWasFreeSpace = isFreeSpace;
	
	return JVMTI_ITERATION_CONTINUE;
}

/**
 * @todo Provide function documentation
 */
bool
tgcDumpInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	bool result = true;

	J9HookInterface** privateHooks = J9_HOOK_INTERFACE(extensions->privateHookInterface);
	(*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_SWEEP_END, tgcHookGlobalGcSweepEnd, OMR_GET_CALLSITE(), NULL);

	return result;
}
