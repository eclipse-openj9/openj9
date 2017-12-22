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

#include "CheckEngine.hpp"
#include "CheckObjectHeap.hpp"
#include "MemorySubSpace.hpp"
#include "ModronTypes.hpp"
#include "ScanFormatter.hpp"
#include "HeapIteratorAPI.h"

/**
 * Private struct used as the user data for the iterator callbacks. The regionDesc will get set
 * by the region iterator callback.
 */
typedef struct ObjectIteratorCallbackUserData {
	GC_CheckEngine* engine; /* Input */
	J9PortLibrary* portLibrary; /* Input */
	J9MM_IterateRegionDescriptor* regionDesc; /* Temp - used internally by iterator functions */
} ObjectIteratorCallbackUserData;

/**
 * Iterator callbacks, these are chained to eventually get to objects and their regions.
 */
static jvmtiIterationControl check_heapIteratorCallback(J9JavaVM* vm, J9MM_IterateHeapDescriptor* heapDesc, void* userData);
static jvmtiIterationControl check_spaceIteratorCallback(J9JavaVM* vm, J9MM_IterateSpaceDescriptor* spaceDesc, void* userData);
static jvmtiIterationControl check_regionIteratorCallback(J9JavaVM* vm, J9MM_IterateRegionDescriptor* regionDesc, void* userData);
static jvmtiIterationControl check_objectIteratorCallback(J9JavaVM* vm, J9MM_IterateObjectDescriptor* objectDesc, void* userData);

GC_Check *
GC_CheckObjectHeap::newInstance(J9JavaVM *javaVM, GC_CheckEngine *engine)
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(javaVM)->getForge();
	
	GC_CheckObjectHeap *check = (GC_CheckObjectHeap *) forge->allocate(sizeof(GC_CheckObjectHeap), MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
	if(check) {
		new(check) GC_CheckObjectHeap(javaVM, engine);
	}
	return check;
}

void
GC_CheckObjectHeap::kill()
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(_javaVM)->getForge();
	forge->free(this);
}

void
GC_CheckObjectHeap::check()
{
	/* Check by using the HeapIteratorAPI */
	ObjectIteratorCallbackUserData userData;
	userData.engine = _engine;
	userData.portLibrary = _portLibrary;
	userData.regionDesc = NULL;
	_javaVM->memoryManagerFunctions->j9mm_iterate_heaps(_javaVM, _portLibrary, 0, check_heapIteratorCallback, &userData);
}

void
GC_CheckObjectHeap::print()
{
	PORT_ACCESS_FROM_PORT(_portLibrary);
	j9tty_printf(PORTLIB, "Printing of the object heap is supported through -Xtgc:terse\n");
}

static jvmtiIterationControl
check_heapIteratorCallback(J9JavaVM* vm, J9MM_IterateHeapDescriptor* heapDesc, void* userData)
{
	ObjectIteratorCallbackUserData* castUserData = (ObjectIteratorCallbackUserData*)userData;
	vm->memoryManagerFunctions->j9mm_iterate_spaces(vm, castUserData->portLibrary, heapDesc, 0, check_spaceIteratorCallback, castUserData);
	return JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
check_spaceIteratorCallback(J9JavaVM* vm, J9MM_IterateSpaceDescriptor* spaceDesc, void* userData)
{
	ObjectIteratorCallbackUserData* castUserData = (ObjectIteratorCallbackUserData*)userData;
	vm->memoryManagerFunctions->j9mm_iterate_regions(vm, castUserData->portLibrary, spaceDesc, 0, check_regionIteratorCallback, castUserData);
	return JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
check_regionIteratorCallback(J9JavaVM* vm, J9MM_IterateRegionDescriptor* regionDesc, void* userData)
{
	ObjectIteratorCallbackUserData* castUserData = (ObjectIteratorCallbackUserData*)userData;
	castUserData->regionDesc = regionDesc;
	vm->memoryManagerFunctions->j9mm_iterate_region_objects(vm, castUserData->portLibrary, regionDesc, j9mm_iterator_flag_include_holes, check_objectIteratorCallback, castUserData);
	return JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
check_objectIteratorCallback(J9JavaVM* vm, J9MM_IterateObjectDescriptor* objectDesc, void* userData)
{
	ObjectIteratorCallbackUserData* castUserData = (ObjectIteratorCallbackUserData*)userData;
	if (castUserData->engine->checkObjectHeap(vm, objectDesc, castUserData->regionDesc) != J9MODRON_SLOT_ITERATOR_OK) {
		return JVMTI_ITERATION_ABORT;
	}
	
	castUserData->engine->pushPreviousObject(objectDesc->object);
	return JVMTI_ITERATION_CONTINUE;
}
