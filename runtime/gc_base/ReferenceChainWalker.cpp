
/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#include "ReferenceChainWalker.hpp"

#include "j9cfg.h"
#include "j9consts.h"
#include "omrgcconsts.h"
#include "j9cp.h"
#include "j9port.h"
#include "modron.h"
#include "omr.h"

#include "ClassIteratorClassSlots.hpp"
#include "ClassIteratorDeclarationOrder.hpp"
#include "EnvironmentBase.hpp"
#include "Forge.hpp"
#include "GCExtensions.hpp"
#include "Heap.hpp"
#include "HeapRegionIterator.hpp"
#include "MixedObjectDeclarationOrderIterator.hpp"
#include "ModronAssertions.h"
#include "ObjectHeapBufferedIterator.hpp"
#include "ObjectModel.hpp"
#include "PointerArrayIterator.hpp"
#include "SlotObject.hpp"
#include "VMThreadIterator.hpp"

class GC_HashTableIterator;
class GC_JVMTIObjectTagTableIterator;
class GC_VMClassSlotIterator;
class MM_HeapRegionDescriptor;
class MM_HeapRegionManager;
class MM_OwnableSynchronizerObjectList;
class MM_UnfinalizedObjectList;

#define TEMP_RCW_STACK_SIZE (10 * 1024 * 1024)

extern "C" {
/**
 * Walk the reference chains starting at the root set, call user provided function.
 * The user function will be called for each root with the type set to one of the
 * J9GC_ROOT_TYPE_* values.  A root may appear multiple times.  The user function
 * will be called for each reference once, with the type set to one of the
 * J9GC_REFERENCE_TYPE_* values.
 * @note This function requires the caller to already have exclusive.
 * @param vmThread
 * @param userCallback User function to be run for each root and reference
 * @param userData Pointer to storage for userData.
 */
void
j9gc_ext_reachable_objects_do(
	J9VMThread *vmThread,
	J9MODRON_REFERENCE_CHAIN_WALKER_CALLBACK userCallback,
	void *userData,
	UDATA walkFlags)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);

	/* Make sure the heap is walkable (flush TLH's, secure heap integrity) */
	vmThread->javaVM->memoryManagerFunctions->j9gc_flush_caches_for_walk(vmThread->javaVM);

	MM_ReferenceChainWalker referenceChainWalker(env, TEMP_RCW_STACK_SIZE, userCallback, userData);
	if (referenceChainWalker.initialize(env)) {
#if defined(J9VM_OPT_JVMTI)
		referenceChainWalker.setIncludeJVMTIObjectTagTables(0 == (walkFlags & J9_MU_WALK_SKIP_JVMTI_TAG_TABLES));
#endif /* J9VM_OPT_JVMTI */
		referenceChainWalker.setTrackVisibleStackFrameDepth(0 != (walkFlags & J9_MU_WALK_TRACK_VISIBLE_FRAME_DEPTH));
		referenceChainWalker.setPreindexInterfaceFields(0 != (walkFlags & J9_MU_WALK_PREINDEX_INTERFACE_FIELDS));
		/* walker configuration complete.  Scan objects... */
		referenceChainWalker.scanReachableObjects(env);
		referenceChainWalker.tearDown(env);
	}
}

/**
 * Walk the reference chains starting at an object, call user provided function.
 * The user function will be called for each reference once, with the type set to
 * one of the J9GC_REFERENCE_TYPE_* values.  An objects class and class loader are
 * considered reachable.
 * @note This function requires the caller to already have exclusive.
 * @param vmThread
 * @param userCallback User function to be run for each root and reference
 * @param userData Pointer to storage for userData.
 */
void
j9gc_ext_reachable_from_object_do(
	J9VMThread *vmThread,
	J9Object *objectPtr,
	J9MODRON_REFERENCE_CHAIN_WALKER_CALLBACK userCallback,
	void *userData,
	UDATA walkFlags)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);

	/* Make sure the heap is walkable (flush TLH's, secure heap integrity) */
	vmThread->javaVM->memoryManagerFunctions->j9gc_flush_caches_for_walk(vmThread->javaVM);

	MM_ReferenceChainWalker referenceChainWalker(env, TEMP_RCW_STACK_SIZE, userCallback, userData);
	if (referenceChainWalker.initialize(env)) {
		referenceChainWalker.setPreindexInterfaceFields(0 != (walkFlags & J9_MU_WALK_PREINDEX_INTERFACE_FIELDS));
		/* walker configuration complete.  Scan objects... */
		referenceChainWalker.scanReachableFromObject(env, objectPtr);
		referenceChainWalker.tearDown(env);
	}
}
} /* extern "C" */

/**
 * Initialized the internal structures.
 * @param env
 * @return true if successfully initialized.
 */
bool
MM_ReferenceChainWalker::initialize(MM_EnvironmentBase *env)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	_heap = _extensions->heap;
	_heapBase = _heap->getHeapBase();
	_heapTop = _heap->getHeapTop();
	if (NULL == extensions->referenceChainWalkerMarkMap) {
		/* first call - create a new Mark Map and initialize it */
		_markMap = MM_ReferenceChainWalkerMarkMap::newInstance(env, _heap->getMaximumPhysicalRange());
		if (NULL != _markMap) {
			extensions->referenceChainWalkerMarkMap = _markMap;
		}
	} else {
		/* mark map is already created, just zero it */
		_markMap = extensions->referenceChainWalkerMarkMap;
		_markMap->clearMap(env);
	}

	bool result = (NULL != _markMap);

	if (result) {
		_queue = (J9Object **)env->getForge()->allocate(_queueSlots * sizeof(J9Object **), MM_AllocationCategory::REFERENCES, J9_GET_CALLSITE());
		if(NULL != _queue) {
			_queueEnd = _queue + _queueSlots;
			_queueCurrent = _queue;
		} else {
			result = false;
		}
	}
	return result;
}

/**
 * Cleans up the internal structures.
 * @param env
 */
void
MM_ReferenceChainWalker::tearDown(MM_EnvironmentBase *env)
{
	if (NULL != _queue) {
		env->getForge()->free(_queue);
		_queue = NULL;
		_queueEnd = NULL;
		_queueCurrent = NULL;
	}

	/* Mark Map should not be released here and stay active - will be released in MM_GCExtensions::tearDown */
}

/**
 * Main handler for each slot.  Calls the <code>_userCallback</code> for each slot
 * and adds the slot to queue for scanning.
 * @param slotPtr pointer to the slot
 * @param type root or reference type (J9GC_ROOT_TYPE_* or J9GC_REFERENCE_TYPE_*)
 * @param index an index identifying the slot within the sourceObj
 * @param sourceObj the object which contains this slot, or NULL if a root
 */
void
MM_ReferenceChainWalker::doSlot(J9Object **slotPtr, IDATA type, IDATA index, J9Object *sourceObj)
{
	jvmtiIterationControl returnCode;
	J9Object *objectPtr = *slotPtr;

	if ((NULL == objectPtr) || _isTerminating) {
		return;
	}

	/* call the call back before the object is corrupted */
	returnCode = _userCallback(slotPtr, sourceObj, _userData, type, index, isMarked(objectPtr) ? 1 : 0);

	if (JVMTI_ITERATION_CONTINUE == returnCode) {
		pushObject(objectPtr);
	} else if (JVMTI_ITERATION_ABORT == returnCode) {
		_isTerminating = true;
		clearQueue();
	}
}

/**
 * Main handler for each class slot.  Calls the <code>_userCallback</code> for each slot
 * and adds the slot to queue for scanning.
 * @param slotPtr pointer to the class slot
 * @param type root or reference type (J9GC_ROOT_TYPE_* or J9GC_REFERENCE_TYPE_*)
 * @param index an index identifying the slot within the sourceObj
 * @param sourceObj the object which contains this slot, or NULL if a root
 */
void
MM_ReferenceChainWalker::doClassSlot(J9Class **slotPtr, IDATA type, IDATA index, J9Object *sourceObj)
{
	if (*slotPtr != NULL) {
		/* NOTE: this "O-slot" is actually a pointer to a local variable in this
		 * function. As such, any changes written back into it will be lost.
		 * Since no-one writes back into the slot in classes-on-heap VMs this is
		 * OK. We should simplify this code once classes-on-heap is enabled
		 * everywhere.
		 */
		J9Object* clazzObject = J9VM_J9CLASS_TO_HEAPCLASS(*slotPtr);
		doSlot(&clazzObject, type, index, sourceObj);
	}
}

/**
 * Main handler for field slots. Decodes the field and forwards to <code>doSlot</code>.
 * If the slot is modified by the callback, stores the modified value back into the field.
 * @param slotPtr pointer to the field slot
 * @param type root or reference type (J9GC_ROOT_TYPE_* or J9GC_REFERENCE_TYPE_*)
 * @param index an index identifying the slot within the sourceObj
 * @param sourceObj the object which contains this slot
 */
void
MM_ReferenceChainWalker::doFieldSlot(GC_SlotObject *slotObject, IDATA type, IDATA index, J9Object *sourceObj)
{
	/* decode the field value into an object pointer */
	j9object_t fieldValue = slotObject->readReferenceFromSlot();

	doSlot(&fieldValue, type, index, sourceObj);

	/* write the value back into the field in case the callback changed it */
	slotObject->writeReferenceToSlot(fieldValue);
}

/**
 * Clears the queue
 */
void
MM_ReferenceChainWalker::clearQueue()
{
	_queueCurrent = _queue;
	_hasOverflowed = false;
}

/**
 * Adds the object to the queue for scanning of its slots
 * @param objectPtr the object to add to the queue
 */
void
MM_ReferenceChainWalker::pushObject(J9Object *objectPtr)
{
	/* only add the object to the queue if it hasn't been scanned and isn't already queued */
	if (!isMarked(objectPtr)) {
		/* ensure there is room in the queue */
		if (_queueCurrent < _queueEnd) {
			setMarked(objectPtr);
			*_queueCurrent = objectPtr;
			_queueCurrent += 1;
		} else {
			/* if we overflow, dump half the objects from the queue and tag them for finding later */
			_hasOverflowed = true;
			setOverflow(objectPtr);
			for (UDATA i = _queueSlots / 2; i > 0; i--) {
				J9Object *objPtr = popObject();
				setOverflow(objPtr);
			}
		}
	}
}

/**
 * Get the next object off the queue for processing
 * @return NULL if the queue is empty
 */
J9Object *
MM_ReferenceChainWalker::popObject()
{
	J9Object *objectPtr;

	if (_queueCurrent == _queue) {
		/* the queue is empty, check if there are overflow objects */
		while (_hasOverflowed && !_isProcessingOverflow) {
			/* We can completely clear out the overflow now.  Field ordering has been keep
			 * via the ordering of doSlot calls.  This is done here as it keeps the queue
			 * and overflow separate from the scanning.
			 */

			_isProcessingOverflow = true;

			/* Reset the overflow flag, so we can determine if we overflow again */
			_hasOverflowed = false;

			findOverflowObjects();

			_isProcessingOverflow = false;
		}
		return NULL;
	}

	_queueCurrent--;
	objectPtr = *_queueCurrent;
	return objectPtr;
}

/**
 * Scan the remaining objects on the queue.  On return the queue will be empty.
 */
void
MM_ReferenceChainWalker::completeScan()
{
	J9Object *objectPtr;

	while (NULL != (objectPtr = popObject())) {
		scanObject(objectPtr);
	}
}

/**
 * Scan an object's slots.  This will also add the reference to the objects class.
 * @param objectPtr the object to scan
 */
void
MM_ReferenceChainWalker::scanObject(J9Object *objectPtr)
{
	/* add the object's class for scanning */
	J9Class* clazz = J9GC_J9OBJECT_CLAZZ(objectPtr);
	doClassSlot(&clazz, J9GC_REFERENCE_TYPE_CLASS, -1, objectPtr);

	switch(_extensions->objectModel.getScanType(objectPtr)) {
	case GC_ObjectModel::SCAN_ATOMIC_MARKABLE_REFERENCE_OBJECT:
	case GC_ObjectModel::SCAN_MIXED_OBJECT:
	case GC_ObjectModel::SCAN_OWNABLESYNCHRONIZER_OBJECT:
	case GC_ObjectModel::SCAN_CLASS_OBJECT:
	case GC_ObjectModel::SCAN_CLASSLOADER_OBJECT:
		scanMixedObject(objectPtr);
		break;
	case GC_ObjectModel::SCAN_POINTER_ARRAY_OBJECT:
		scanPointerArrayObject((J9IndexableObject *)objectPtr);
		break;
	case GC_ObjectModel::SCAN_REFERENCE_MIXED_OBJECT:
		scanReferenceMixedObject(objectPtr);
		break;
	case GC_ObjectModel::SCAN_PRIMITIVE_ARRAY_OBJECT:
		/* nothing to do */
		break;
	default:
		Assert_MM_unreachable();
	}

	if (J9GC_IS_INITIALIZED_HEAPCLASS((J9VMThread*)_env->getLanguageVMThread(), objectPtr)) {
		scanClass(J9VM_J9CLASS_FROM_HEAPCLASS((J9VMThread*)_env->getLanguageVMThread(), objectPtr));
	}
}

/**
 * @todo Provide function documentation
 */
void
MM_ReferenceChainWalker::scanMixedObject(J9Object *objectPtr)
{
	GC_MixedObjectDeclarationOrderIterator objectIterator(static_cast<J9JavaVM*>(_omrVM->_language_vm), objectPtr, _shouldPreindexInterfaceFields);

	while(GC_SlotObject *slotObject = objectIterator.nextSlot()) {
		doFieldSlot(slotObject, J9GC_REFERENCE_TYPE_FIELD, objectIterator.getIndex(), objectPtr);
	}
}

/**
 * @todo Provide function documentation
 */
void
MM_ReferenceChainWalker::scanPointerArrayObject(J9IndexableObject *objectPtr)
{
	GC_PointerArrayIterator pointerArrayIterator(static_cast<J9JavaVM*>(_omrVM->_language_vm), (J9Object *)objectPtr);

	while(GC_SlotObject *slotObject = pointerArrayIterator.nextSlot()) {
		doFieldSlot(slotObject, J9GC_REFERENCE_TYPE_ARRAY, pointerArrayIterator.getIndex(), (J9Object *)objectPtr);
	}
}

/**
 * @todo Provide function documentation
 */
void
MM_ReferenceChainWalker::scanReferenceMixedObject(J9Object *objectPtr)
{
	GC_MixedObjectDeclarationOrderIterator objectIterator(static_cast<J9JavaVM*>(_omrVM->_language_vm), objectPtr, _shouldPreindexInterfaceFields);

	while(GC_SlotObject *slotObject = objectIterator.nextSlot()) {
		doFieldSlot(slotObject, J9GC_REFERENCE_TYPE_WEAK_REFERENCE, objectIterator.getIndex(), objectPtr);
	}
}

/**
 * @todo Provide function documentation
 */
void
MM_ReferenceChainWalker::scanClass(J9Class *clazz)
{
	J9Object* referrer = J9VM_J9CLASS_TO_HEAPCLASS(clazz);

	GC_ClassIteratorDeclarationOrder classIterator(static_cast<J9JavaVM*>(_omrVM->_language_vm), clazz, _shouldPreindexInterfaceFields);
	while(volatile j9object_t *slot = classIterator.nextSlot()) {
		IDATA refType = classIterator.getSlotReferenceType();
		IDATA index = classIterator.getIndex();

		/* discard volatile since we must be in stop-the-world mode */
		doSlot((j9object_t*)slot, refType, index, referrer);
	}

	GC_ClassIteratorClassSlots classIteratorClassSlots(clazz);
	while(J9Class **slot = classIteratorClassSlots.nextSlot()) {
		switch (classIteratorClassSlots.getState()) {
		case classiteratorclassslots_state_constant_pool:
			doClassSlot(slot, J9GC_REFERENCE_TYPE_CONSTANT_POOL, classIteratorClassSlots.getIndex(), referrer);
			break;
		case classiteratorclassslots_state_superclasses:
			doClassSlot(slot, J9GC_REFERENCE_TYPE_SUPERCLASS, classIteratorClassSlots.getIndex(), referrer);
			break;
		case classiteratorclassslots_state_interfaces:
			doClassSlot(slot, J9GC_REFERENCE_TYPE_INTERFACE, classIteratorClassSlots.getIndex(), referrer);
			break;
		case classiteratorclassslots_state_array_class_slots:
			doClassSlot(slot, J9GC_REFERENCE_TYPE_CLASS_ARRAY_CLASS, classIteratorClassSlots.getIndex(), referrer);
			break;
		default:
			doClassSlot(slot, J9GC_REFERENCE_TYPE_UNKNOWN, classIteratorClassSlots.getIndex(), referrer);
			break;
		}
	}

	/* GC_ClassIteratorDeclarationOrder does not iterate the classLoader slot, we want to report the reference */
	J9Object **slot = J9GC_J9CLASSLOADER_CLASSLOADEROBJECT_EA(clazz->classLoader);
	doSlot(slot, J9GC_REFERENCE_TYPE_CLASSLOADER, -1, referrer);
}

/**
 * Finds objects in the heap that were overflowed, adds them back to the queue, and completes a scan.
 */
void
MM_ReferenceChainWalker::findOverflowObjects()
{
	MM_Heap *heap = _extensions->heap;
	MM_HeapRegionManager *regionManager = heap->getHeapRegionManager();
	GC_HeapRegionIterator regionIterator(regionManager);

	MM_HeapRegionDescriptor *region = NULL;
	while(NULL != (region = regionIterator.nextRegion())) {
		GC_ObjectHeapBufferedIterator objectHeapIterator(_extensions, region); 

		J9Object *object = NULL;
		while((object = objectHeapIterator.nextObject()) != NULL) {
			if (isOverflow(object)) {
				clearOverflow(object);
				pushObject(object);
				completeScan();
			}
		}
	}
}

/**
 * @todo Provide function documentation
 */
void
MM_ReferenceChainWalker::doSlot(J9Object **slotPtr) {
	doSlot(slotPtr, J9GC_ROOT_TYPE_UNKNOWN, -1, NULL);
}

/**
 * Process a class slot pointer.
 * @param slotPtr a pointer to a class slot
 */
void
MM_ReferenceChainWalker::doClassSlot(J9Class** slotPtr)
{
	doClassSlot(slotPtr, J9GC_ROOT_TYPE_CLASS, -1, NULL);
}

/**
 * @todo Provide function documentation
 */
void
MM_ReferenceChainWalker::doClass(J9Class *clazz) {
	doClassSlot(&clazz);
}


/**
 * @todo Provide function documentation
 */
void
MM_ReferenceChainWalker::doClassLoader(J9ClassLoader *classLoader)
{
	doSlot(J9GC_J9CLASSLOADER_CLASSLOADEROBJECT_EA(classLoader), J9GC_ROOT_TYPE_CLASSLOADER, -1, NULL);
}

#if defined(J9VM_GC_FINALIZATION)
/**
 * @todo Provide function documentation
 */
void
MM_ReferenceChainWalker::doUnfinalizedObject(J9Object *objectPtr, MM_UnfinalizedObjectList *list)
{
	J9Object *object = objectPtr;
	doSlot(&object, J9GC_ROOT_TYPE_UNFINALIZED_OBJECT, -1, NULL);
}
#endif /* J9VM_GC_FINALIZATION */

void
MM_ReferenceChainWalker::doOwnableSynchronizerObject(J9Object *objectPtr, MM_OwnableSynchronizerObjectList *list)
{
	J9Object *object = objectPtr;
	doSlot(&object, J9GC_ROOT_TYPE_OWNABLE_SYNCHRONIZER_OBJECT, -1, NULL);
}

/**
 * @todo Provide function documentation
 */
void
MM_ReferenceChainWalker::doMonitorReference(J9ObjectMonitor *objectMonitor, GC_HashTableIterator *monitorReferenceIterator)
{
	J9ThreadAbstractMonitor * monitor = (J9ThreadAbstractMonitor*)objectMonitor->monitor;
	doSlot((J9Object **)& monitor->userData, J9GC_ROOT_TYPE_MONITOR, -1, NULL);
}

/**
 * @todo Provide function documentation
 */
void
MM_ReferenceChainWalker::doJNIWeakGlobalReference(J9Object **slotPtr)
{
	doSlot(slotPtr, J9GC_ROOT_TYPE_JNI_WEAK_GLOBAL, -1, NULL);
}

#if defined(J9VM_GC_MODRON_SCAVENGER)
/**
 * @todo Provide function documentation
 */
void
MM_ReferenceChainWalker::doRememberedSetSlot(J9Object **slotPtr, GC_RememberedSetSlotIterator *rememberedSetSlotIterator)
{
	doSlot(slotPtr, J9GC_ROOT_TYPE_REMEMBERED_SET, -1, NULL);
}
#endif /* defined(J9VM_GC_MODRON_SCAVENGER) */

/**
 * @todo Provide function documentation
 */
void
MM_ReferenceChainWalker::doStringTableSlot(J9Object **slotPtr, GC_StringTableIterator *stringTableIterator)
{
	doSlot(slotPtr, J9GC_ROOT_TYPE_STRING_TABLE, -1, NULL);
}

/**
 * @todo Provide function documentation
 */
void
MM_ReferenceChainWalker::doVMClassSlot(J9Class **slotPtr, GC_VMClassSlotIterator *vmClassSlotIterator)
{
	doClassSlot(slotPtr, J9GC_ROOT_TYPE_VM_CLASS_SLOT, -1, NULL);
}

/**
 * @todo Provide function documentation
 */
void
MM_ReferenceChainWalker::doStackSlot(J9Object **slotPtr, void *walkState, const void* stackLocation)
{
	J9Object *slotValue = *slotPtr;

	/* Only report heap objects */

	if (isHeapObject(slotValue) && !_heap->objectIsInGap(slotValue)) {
		doSlot(slotPtr, J9GC_ROOT_TYPE_STACK_SLOT, -1, (J9Object *)walkState);
	}
}

#if defined(J9VM_OPT_JVMTI)
/**
 * @todo Provide function documentation
 */
void
MM_ReferenceChainWalker::doJVMTIObjectTagSlot(J9Object **slotPtr, GC_JVMTIObjectTagTableIterator *objectTagTableIterator)
{
	doSlot(slotPtr, J9GC_ROOT_TYPE_JVMTI_TAG_REF, -1, NULL);
}
#endif /* J9VM_OPT_JVMTI */

/**
 * @todo Provide function documentation
 */
void
MM_ReferenceChainWalker::doVMThreadSlot(J9Object **slotPtr, GC_VMThreadIterator *vmThreadIterator)
{
	J9Object *slotValue = *slotPtr;

	switch(vmThreadIterator->getState()) {
	case vmthreaditerator_state_slots:
		doSlot(slotPtr, J9GC_ROOT_TYPE_THREAD_SLOT, -1, NULL);
		break;
	case vmthreaditerator_state_jni_slots:
		doSlot(slotPtr, J9GC_ROOT_TYPE_JNI_LOCAL, -1, NULL);
		break;
#if defined(J9VM_INTERP_HOT_CODE_REPLACEMENT)
	case vmthreaditerator_state_monitor_records:
		if (isHeapObject(slotValue) && !_heap->objectIsInGap(slotValue)) {
			doSlot(slotPtr, J9GC_ROOT_TYPE_THREAD_MONITOR, -1, NULL);
		}
		break;
#endif
	default:
		doSlot(slotPtr, J9GC_ROOT_TYPE_UNKNOWN, -1, NULL);
		break;
	}
}

#if defined(J9VM_GC_FINALIZATION)
/**
 * @todo Provide function documentation
 */
void
MM_ReferenceChainWalker::doFinalizableObject(J9Object *objectPtr)
{
	J9Object *object = objectPtr;
	doSlot(&object, J9GC_ROOT_TYPE_FINALIZABLE_OBJECT, -1, NULL);
}
#endif /* J9VM_GC_FINALIZATION */

/**
 * @todo Provide function documentation
 */
void
MM_ReferenceChainWalker::doJNIGlobalReferenceSlot(J9Object **slotPtr, GC_JNIGlobalReferenceIterator *jniGlobalReferenceIterator)
{
	doSlot(slotPtr, J9GC_ROOT_TYPE_JNI_GLOBAL, -1, NULL);
}
