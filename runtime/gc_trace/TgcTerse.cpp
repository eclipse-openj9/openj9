
/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#include "j9.h"
#include "j9cfg.h"
#include "j9port.h"
#include "modronopt.h"
#include "Tgc.hpp"
#include "mmhook_internal.h"
#include "HeapIteratorAPI.h"

#include "GCExtensions.hpp"
#include "TgcExtensions.hpp"

/**
 * Private struct used as the user data for the iterator callbacks. The regionDesc will get set
 * by the region iterator callback.
 */
typedef struct FixDeadObjectsIteratorCallbackUserData {
	J9MM_IterateRegionDescriptor* regionDesc; /* Temp - used internally by iterator functions */
} FixDeadObjectsIteratorCallbackUserData;

/**
 * Iterator callbacks, these are chained to eventually get to objects and their regions.
 */
static jvmtiIterationControl fix_heapIteratorCallback(J9JavaVM* vm, J9MM_IterateHeapDescriptor* heapDesc, void* userData);
static jvmtiIterationControl fix_spaceIteratorCallback(J9JavaVM* vm, J9MM_IterateSpaceDescriptor* spaceDesc, void* userData);
static jvmtiIterationControl fix_regionIteratorCallback(J9JavaVM* vm, J9MM_IterateRegionDescriptor* regionDesc, void* userData);
static jvmtiIterationControl fix_objectIteratorCallback(J9JavaVM* vm, J9MM_IterateObjectDescriptor* objectDesc, void* userData);
static jvmtiIterationControl dump_objectIteratorCallback(J9JavaVM* vm, J9MM_IterateObjectDescriptor* objectDesc, void *userData);

/**
 * @todo Provide function documentation
 */
static void
dumpHeap(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	TgcTerseExtensions *terseExtensions = &tgcExtensions->_terse;

	/* Flush any VM level changes to prepare for a safe slot walk */
	TRIGGER_J9HOOK_MM_PRIVATE_WALK_HEAP_START(MM_GCExtensions::getExtensions(javaVM)->privateHookInterface, extensions->getOmrVM());

	javaVM->memoryManagerFunctions->j9mm_iterate_all_objects(javaVM, javaVM->portLibrary, j9mm_iterator_flag_include_holes, dump_objectIteratorCallback, terseExtensions);

	TRIGGER_J9HOOK_MM_PRIVATE_WALK_HEAP_END(MM_GCExtensions::getExtensions(javaVM)->privateHookInterface, extensions->getOmrVM());
}

static jvmtiIterationControl
dump_objectIteratorCallback(J9JavaVM *vm, J9MM_IterateObjectDescriptor* objectDesc, void *userData)
{
	TgcTerseExtensions *terseExtensions = (TgcTerseExtensions*)userData;
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(vm);
	
	tgcExtensions->printf("*DH(%d)* %p %s", 
		terseExtensions->gcCount,
		objectDesc->object,
		objectDesc->isObject ? "a" : "f");
    
	if (objectDesc->isObject) {
		tgcExtensions->printf(" x%p ", objectDesc->size);
		tgcPrintClass(vm, J9GC_J9OBJECT_CLAZZ(objectDesc->object));
		tgcExtensions->printf("\n");
	} else {
		tgcExtensions->printf(" x%p\n", objectDesc->size); 
	}  	
	
	return JVMTI_ITERATION_CONTINUE;
}

/**
 * @todo Provide function documentation
 */
static void
tgcHookGlobalGcStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_GlobalGCStartEvent* event = (MM_GlobalGCStartEvent*)eventData;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(event->currentThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	TgcTerseExtensions *terseExtensions = &tgcExtensions->_terse;

	terseExtensions->gcCount += 1;
	/* TODO: Sovereign also prints the size of the allocation that caused the failure */
	tgcExtensions->printf("*** gc(%zu) ***\n", terseExtensions->gcCount);
	dumpHeap((J9JavaVM*)event->currentThread->_vm->_language_vm);
}

/**
 * @todo Provide function documentation
 */
static void
tgcHookGlobalGcEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_GlobalGCEndEvent* event = (MM_GlobalGCEndEvent*)eventData;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(event->currentThread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	TgcTerseExtensions *terseExtensions = &tgcExtensions->_terse;

	tgcExtensions->printf("** gc(%zu) done **\n", terseExtensions->gcCount);
	dumpHeap((J9JavaVM*)event->currentThread->_vm->_language_vm);
}

/**
 * @todo Provide function documentation
 */
static void
tgcHookLocalGcStart(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_LocalGCStartEvent* event = (MM_LocalGCStartEvent*)eventData;
	J9VMThread* vmThread = (J9VMThread*) event->currentThread->_language_vmthread;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(vmThread->javaVM);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	TgcTerseExtensions *terseExtensions = &tgcExtensions->_terse;
   
	terseExtensions->gcCount += 1;
	/* TODO: Sovereign also prints the size of the allocation that caused the failure */
	tgcExtensions->printf("*** gc(%zu) ***\n", terseExtensions->gcCount);
	dumpHeap(vmThread->javaVM);
}

/**
 * @todo Provide function documentation
 */
static void
tgcHookLocalGcEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_LocalGCEndEvent* event = (MM_LocalGCEndEvent*)eventData;
	J9VMThread* vmThread = (J9VMThread*) event->currentThread->_language_vmthread;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(vmThread->javaVM);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(extensions);
	TgcTerseExtensions *terseExtensions = &tgcExtensions->_terse;

	tgcExtensions->printf("** gc(%zu) done **\n", terseExtensions->gcCount);
	dumpHeap(vmThread->javaVM);
}

/**
 * @todo Provide function documentation
 */
static void
tgcHookGlobalGcSweepEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_SweepEndEvent* event = (MM_SweepEndEvent*)eventData;
	J9VMThread *vmThread = static_cast<J9VMThread*>(event->currentThread->_language_vmthread);
	J9JavaVM* javaVM = vmThread->javaVM;
	
	/* Fix up the heap so that it can safely be walked later */
	FixDeadObjectsIteratorCallbackUserData iteratorData;
	iteratorData.regionDesc = NULL;
	javaVM->memoryManagerFunctions->j9mm_iterate_heaps(javaVM, javaVM->portLibrary, 0, fix_heapIteratorCallback, &iteratorData);
}

/**
 * @todo Provide function documentation
 */
bool
tgcTerseInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	bool result = true;

	J9HookInterface** omrHooks = J9_HOOK_INTERFACE(extensions->omrHookInterface);
	J9HookInterface** privateHooks = J9_HOOK_INTERFACE(extensions->privateHookInterface);
	(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_GLOBAL_GC_START, tgcHookGlobalGcStart, OMR_GET_CALLSITE(), NULL);
	(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_GLOBAL_GC_END, tgcHookGlobalGcEnd, OMR_GET_CALLSITE(), NULL);
	(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_LOCAL_GC_START, tgcHookLocalGcStart, OMR_GET_CALLSITE(), NULL);
	(*omrHooks)->J9HookRegisterWithCallSite(omrHooks, J9HOOK_MM_OMR_LOCAL_GC_END, tgcHookLocalGcEnd, OMR_GET_CALLSITE(), NULL);
	(*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_SWEEP_END, tgcHookGlobalGcSweepEnd, OMR_GET_CALLSITE(), NULL);

	return result;
}

static jvmtiIterationControl
fix_heapIteratorCallback(J9JavaVM* vm, J9MM_IterateHeapDescriptor* heapDesc, void* userData)
{
	FixDeadObjectsIteratorCallbackUserData* castUserData = (FixDeadObjectsIteratorCallbackUserData*)userData;
	vm->memoryManagerFunctions->j9mm_iterate_spaces(vm, vm->portLibrary, heapDesc, 0, fix_spaceIteratorCallback, castUserData);
	return JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
fix_spaceIteratorCallback(J9JavaVM* vm, J9MM_IterateSpaceDescriptor* spaceDesc, void* userData)
{
	FixDeadObjectsIteratorCallbackUserData* castUserData = (FixDeadObjectsIteratorCallbackUserData*)userData;
	vm->memoryManagerFunctions->j9mm_iterate_regions(vm, vm->portLibrary, spaceDesc, 0, fix_regionIteratorCallback, castUserData);
	return JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
fix_regionIteratorCallback(J9JavaVM* vm, J9MM_IterateRegionDescriptor* regionDesc, void* userData)
{
	FixDeadObjectsIteratorCallbackUserData* castUserData = (FixDeadObjectsIteratorCallbackUserData*)userData;
	castUserData->regionDesc = regionDesc;
	vm->memoryManagerFunctions->j9mm_iterate_region_objects(vm, vm->portLibrary, regionDesc, 0, fix_objectIteratorCallback, castUserData);
	return JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
fix_objectIteratorCallback(J9JavaVM* vm, J9MM_IterateObjectDescriptor* objectDesc, void* userData)
{
	FixDeadObjectsIteratorCallbackUserData* castUserData = (FixDeadObjectsIteratorCallbackUserData*)userData;

	/* Check the mark state of each object. If it isn't marked, build a dead object. */
	if( !vm->memoryManagerFunctions->j9gc_ext_is_marked(vm, objectDesc->object) ) {
		vm->memoryManagerFunctions->j9mm_abandon_object(vm, castUserData->regionDesc, objectDesc);
	}

	return JVMTI_ITERATION_CONTINUE;
}
