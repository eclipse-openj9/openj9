
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

#if !defined(REFERENCECHAINWALKER_HPP_)
#define REFERENCECHAINWALKER_HPP_

#include "j9accessbarrier.h"
#include "j9cfg.h"
#include "j9comp.h"
#include "j9modron.h"
#include "modronbase.h"

#include "ModronTypes.hpp"
#include "ReferenceChainWalkerMarkMap.hpp"
#include "RootScanner.hpp"
#include "RootScannerTypes.h"

class GC_HashTableIterator;
class GC_JVMTIObjectTagTableIterator;
class GC_SlotObject;
class GC_VMClassSlotIterator;
class GC_VMThreadIterator;
class MM_EnvironmentBase;
class MM_Heap;
class MM_OwnableSynchronizerObjectList;
class MM_UnfinalizedObjectList;

/**
 * Interface for walking reference chains in the system starting either at the root set or a specified object.
 * 
 * MM_ReferenceChainWalker makes use of the general MM_RootScanner for walking the root set.  It uses a user
 * callback to provide the function to be done on all references and roots.  The type of reference is reported back
 * to the callback functions.
 *
 * The objects are queued in a stack that makes use of the OBJECT_HEADER_COLOR_MASK bits of the object header
 * flags to indicate whether an object has been queued, or overflowed.  The stack is allocated as an array of slots
 * of the size passed in to the constructor.  When the stack runs out of space, half of the objects are removed
 * and have their overflow flag set.  These objects are scanned for when the stack next becomes empty.
 * 
 * @note You must use <code>initialize</code> and <code>kill</code> to allocate and deallocate the internal structures.
 * @note Does not handle overflowing stack allocated objects.
 * @ingroup GC_Base
 */
class MM_ReferenceChainWalker : public MM_RootScanner
{
private:
	J9Object **_queue; /**< Main queue used for queuing objects */
	J9Object **_queueEnd; /**< End of the queue used for queuing objects */
	J9Object **_queueCurrent; /**< Current position of the queue used for queuing objects */
	UDATA _queueSlots; /**< Size of the queue before overflow occurs */
	J9MODRON_REFERENCE_CHAIN_WALKER_CALLBACK *_userCallback; /**< User callback function to be called on every reachable slot */
	void *_userData; /**< User data to be passed to the user callback function */
	bool _hasOverflowed; /**< Set when the queue has overflowed */
	bool _isProcessingOverflow; /**< Set when the queue is currently processing the overflow */
	bool _isTerminating; /**< Set when no more callbacks should be queued */
	bool _shouldPreindexInterfaceFields; /**< if true, indexes interface fields of the class being visited before class and superclass fields, otherwise, returns them in the order they appear in an object instance (CMVC 142897) */
	MM_ReferenceChainWalkerMarkMap *_markMap;	/**< Mark Map created for Reference Chain Walker */
	MM_Heap *_heap; /**< Cached pointer to the heap */
	void *_heapBase; /**< Cached value of the heap base */
	void *_heapTop; /**< Cached value of the heap top */

	void clearQueue();
	void pushObject(J9Object *obj);
	J9Object *popObject();

	void findOverflowObjects();
	
	virtual void scanClass(J9Class *clazz);
	virtual void scanObject(J9Object *objectPtr);
	virtual void scanMixedObject(J9Object *objectPtr);
	virtual void scanPointerArrayObject(J9IndexableObject *objectPtr);
	virtual void scanReferenceMixedObject(J9Object *objectPtr);
	
	virtual void doClassLoader(J9ClassLoader *classLoader);

#if defined(J9VM_GC_FINALIZATION)
	virtual void doUnfinalizedObject(J9Object *objectPtr, MM_UnfinalizedObjectList *list);
	virtual void doFinalizableObject(J9Object *objectPtr);
#endif /* J9VM_GC_FINALIZATION */

	/**
	 * @todo Provide function documentation
	 */
	virtual void doOwnableSynchronizerObject(J9Object *objectPtr, MM_OwnableSynchronizerObjectList *list);

	virtual void doJNIWeakGlobalReference(J9Object **slotPtr);
	virtual void doJNIGlobalReferenceSlot(J9Object **slotPtr, GC_JNIGlobalReferenceIterator *jniGlobalReferenceIterator);

#if defined(J9VM_GC_MODRON_SCAVENGER)
	virtual void doRememberedSetSlot(J9Object **slotPtr, GC_RememberedSetSlotIterator *rememberedSetSlotIterator);
#endif /* defined(J9VM_GC_MODRON_SCAVENGER) */

#if defined(J9VM_OPT_JVMTI)
	virtual void doJVMTIObjectTagSlot(J9Object **slotPtr, GC_JVMTIObjectTagTableIterator *objectTagTableIterator);
#endif /* J9VM_OPT_JVMTI */

	virtual void doMonitorReference(J9ObjectMonitor *objectMonitor, GC_HashTableIterator *monitorReferenceIterator);
	virtual void doStringTableSlot(J9Object **slotPtr, GC_StringTableIterator *stringTableIterator);
	virtual void doVMClassSlot(J9Class **slotPtr, GC_VMClassSlotIterator *vmClassSlotIterator);
	virtual void doVMThreadSlot(J9Object **slotPtr, GC_VMThreadIterator *vmThreadIterator);
	virtual void doStackSlot(J9Object **slotPtr, void *walkState, const void* stackLocation);
	virtual void doSlot(J9Object **slotPtr);
	virtual void doClassSlot(J9Class **slotPtr);
	virtual void doClass(J9Class *clazz);
	virtual void doSlot(J9Object **slotPtr, IDATA type, IDATA index, J9Object *sourceObj);
	virtual void doClassSlot(J9Class **slotPtr, IDATA type, IDATA index, J9Object *sourceObj);
	using MM_RootScanner::doFieldSlot;
	virtual void doFieldSlot(GC_SlotObject *slotObject, IDATA type, IDATA index, J9Object *sourceObj);
	
	MMINLINE virtual CompletePhaseCode scanClassesComplete(MM_EnvironmentBase *env) {
		reportScanningStarted(RootScannerEntity_ClassesComplete);
		completeScan();
		reportScanningEnded(RootScannerEntity_ClassesComplete);
		return complete_phase_OK;
	}

	MMINLINE virtual CompletePhaseCode scanWeakReferencesComplete(MM_EnvironmentBase *env) {
		return complete_phase_OK;
	}
	MMINLINE virtual CompletePhaseCode scanSoftReferencesComplete(MM_EnvironmentBase *env) {
		return complete_phase_OK;
	}

	MMINLINE virtual CompletePhaseCode scanPhantomReferencesComplete(MM_EnvironmentBase *env) {
		return complete_phase_OK;
	}

#if defined(J9VM_GC_FINALIZATION)
	MMINLINE virtual CompletePhaseCode scanUnfinalizedObjectsComplete(MM_EnvironmentBase *env) {
		reportScanningStarted(RootScannerEntity_UnfinalizedObjectsComplete);
		completeScan();
		reportScanningEnded(RootScannerEntity_UnfinalizedObjectsComplete);
		return complete_phase_OK;
	}
#endif /* J9VM_GC_FINALIZATION */

	virtual CompletePhaseCode scanMonitorReferencesComplete(MM_EnvironmentBase *env) {
		return complete_phase_OK;
	}

	void completeScan();


	MMINLINE bool isHeapObject(J9Object* objectPtr)
	{
		return ((_heapBase <= (U_8 *)objectPtr) && (_heapTop > (U_8 *)objectPtr));
	}


	MMINLINE bool isMarked(J9Object *object)
	{
		if (isHeapObject(object)) {
			/* check one bit in mark map only - return true if 10 or 11 */
			return _markMap->isBitSet(object);
		} else {
			return true;
		}
	}

	MMINLINE void setMarked(J9Object * object)
	{
		if (isHeapObject(object)) {
			/* mark one bit only (next one has to be 0) - set 10 */
			_markMap->setBit(object);
		}
	}

	MMINLINE void setOverflow(J9Object *object)
	{
		if (isHeapObject(object)) {
			/* set both mark bits - set 11 */
			_markMap->setBit(object);
			_markMap->setBit((J9Object *)((UDATA)object + sizeof(fj9object_t)));
		}
	}

	MMINLINE bool isOverflow(J9Object * object)
	{
		if (isHeapObject(object)) {
			/* check both bits in mark map - return true if 11 */
			return (_markMap->isBitSet(object) && _markMap->isBitSet((J9Object *)((UDATA)object + sizeof(fj9object_t))));
		} else {
			return false;
		}
	}

	MMINLINE void clearOverflow(J9Object * object)
	{
		if (isHeapObject(object)) {
			/* clear both mark bits - set 00 */
			_markMap->clearBit(object);
			_markMap->clearBit((J9Object *)((UDATA)object + sizeof(fj9object_t)));
		}
	}

protected:

public:
	MM_ReferenceChainWalker(MM_EnvironmentBase *env, UDATA queueSlots, J9MODRON_REFERENCE_CHAIN_WALKER_CALLBACK *userCallback, void *userData) :
		MM_RootScanner(env, true),
		_queue(NULL),
		_queueEnd(NULL),
		_queueCurrent(NULL),
		_queueSlots(queueSlots),
		_userCallback(userCallback),
		_userData(userData),
		_hasOverflowed(false),
		_isProcessingOverflow(false),
		_isTerminating(false),
		_shouldPreindexInterfaceFields(true),	/* default to behaviour required for Java6/heap11 */
		_markMap(NULL),
		_heap(NULL),
		_heapBase(NULL),
		_heapTop(NULL)
	{
		_typeId = __FUNCTION__;
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		setClassDataAsRoots(false);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	};

	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);

	void scanReachableObjects(MM_EnvironmentBase *env) {
		scanAllSlots(env);
		completeScan();
	}

	void scanReachableFromObject(MM_EnvironmentBase *env, J9Object *objectPtr) {
		pushObject(objectPtr);
		completeScan();
	}
	
	/**
	 * Added to support bi-modal interface indexing in JVMTI (CMVC 142897).
	 * Detail:  heap10 requires no pre-indexing in order to preserve Java5 behaviour but heap11 requires pre-indexing to pass a Java6 JCK
	 */
	void setPreindexInterfaceFields(bool shouldPreindexInterfaceFields) { _shouldPreindexInterfaceFields =  shouldPreindexInterfaceFields; }
};

#endif /* REFERENCECHAINWALKER_HPP_ */

