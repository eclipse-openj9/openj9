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
 
#include "j9cfg.h"

#include "CheckEngine.hpp"
#include "FixDeadObjects.hpp"
#include "HeapIteratorAPI.h"
#include "ModronTypes.hpp"
#include "ScanFormatter.hpp"

/**
 * Private struct used as the user data for the iterator callbacks. The regionDesc will get set
 * by the region iterator callback.
 */
typedef struct FixDeadObjectsIteratorCallbackUserData {
	GC_CheckEngine* engine; /* Input */
	J9PortLibrary* portLibrary; /* Input */
	J9MM_IterateRegionDescriptor* regionDesc; /* Temp - used internally by iterator functions */
} FixDeadObjectsIteratorCallbackUserData;

/**
 * Iterator callbacks, these are chained to eventually get to objects and their regions.
 */
static jvmtiIterationControl fix_heapIteratorCallback(J9JavaVM* vm, J9MM_IterateHeapDescriptor* heapDesc, void* userData);
static jvmtiIterationControl fix_spaceIteratorCallback(J9JavaVM* vm, J9MM_IterateSpaceDescriptor* spaceDesc, void* userData);
static jvmtiIterationControl fix_regionIteratorCallback(J9JavaVM* vm, J9MM_IterateRegionDescriptor* regionDesc, void* userData);
static jvmtiIterationControl fix_objectIteratorCallback(J9JavaVM* vm, J9MM_IterateObjectDescriptor* objectDesc, void* userData);


GC_Check *
GC_FixDeadObjects::newInstance(J9JavaVM *javaVM, GC_CheckEngine *engine)
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(javaVM)->getForge();
	
	GC_FixDeadObjects *check = (GC_FixDeadObjects *) forge->allocate(sizeof(GC_FixDeadObjects), MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
	if(check) {
		new(check) GC_FixDeadObjects(javaVM, engine);
	}
	return check;
}

void
GC_FixDeadObjects::kill()
{
	MM_Forge *forge = MM_GCExtensions::getExtensions(_javaVM)->getForge();
	forge->free(this);
}

void
GC_FixDeadObjects::check()
{
	/* Check by using the HeapIteratorAPI */
	FixDeadObjectsIteratorCallbackUserData userData;
	userData.engine = _engine;
	userData.portLibrary = _portLibrary;
	_javaVM->memoryManagerFunctions->j9mm_iterate_heaps(_javaVM, _portLibrary, 0, fix_heapIteratorCallback, &userData);
}

void
GC_FixDeadObjects::print()
{
}

static jvmtiIterationControl
fix_heapIteratorCallback(J9JavaVM* vm, J9MM_IterateHeapDescriptor* heapDesc, void* userData)
{
	FixDeadObjectsIteratorCallbackUserData* castUserData = (FixDeadObjectsIteratorCallbackUserData*)userData;
	vm->memoryManagerFunctions->j9mm_iterate_spaces(vm, castUserData->portLibrary, heapDesc, 0, fix_spaceIteratorCallback, castUserData);
	return JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
fix_spaceIteratorCallback(J9JavaVM* vm, J9MM_IterateSpaceDescriptor* spaceDesc, void* userData)
{
	FixDeadObjectsIteratorCallbackUserData* castUserData = (FixDeadObjectsIteratorCallbackUserData*)userData;
	vm->memoryManagerFunctions->j9mm_iterate_regions(vm, castUserData->portLibrary, spaceDesc, 0, fix_regionIteratorCallback, castUserData);
	return JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
fix_regionIteratorCallback(J9JavaVM* vm, J9MM_IterateRegionDescriptor* regionDesc, void* userData)
{
	FixDeadObjectsIteratorCallbackUserData* castUserData = (FixDeadObjectsIteratorCallbackUserData*)userData;
	castUserData->regionDesc = regionDesc;
	vm->memoryManagerFunctions->j9mm_iterate_region_objects(vm, castUserData->portLibrary, regionDesc, 0, fix_objectIteratorCallback, castUserData);
	return JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
fix_objectIteratorCallback(J9JavaVM* vm, J9MM_IterateObjectDescriptor* objectDesc, void* userData)
{
	FixDeadObjectsIteratorCallbackUserData* castUserData = (FixDeadObjectsIteratorCallbackUserData*)userData;

	/* Check the mark state of each object. If it isn't marked, build a dead object. */
	if( !vm->memoryManagerFunctions->j9gc_ext_is_marked(vm, objectDesc->object) ) {
		vm->memoryManagerFunctions->j9mm_abandon_object(vm, castUserData->regionDesc, objectDesc);
	} else if (castUserData->engine->checkObjectHeap(vm, objectDesc, castUserData->regionDesc) != J9MODRON_SLOT_ITERATOR_OK) {
		return JVMTI_ITERATION_ABORT;
	}
	
	castUserData->engine->pushPreviousObject(objectDesc->object);
	return JVMTI_ITERATION_CONTINUE;
}
