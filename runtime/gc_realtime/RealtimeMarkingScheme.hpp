/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#if !defined(REALTIMEMARKINGSCHEME_HPP_)
#define REALTIMEMARKINGSCHEME_HPP_

#include "j9.h"

#include "Base.hpp"
#include "EnvironmentRealtime.hpp"
#include "GCExtensions.hpp"
#include "MarkMap.hpp"
#include "RealtimeGC.hpp"

#include "SegregatedMarkingScheme.hpp"

class MM_RealtimeMarkingSchemeRootMarker;
class MM_RealtimeRootScanner;
class MM_Scheduler;

#define REFERENCE_OBJECT_YIELD_CHECK_INTERVAL 200
#define UNFINALIZED_OBJECT_YIELD_CHECK_INTERVAL 70
#define OWNABLE_SYNCHRONIZER_OBJECT_YIELD_CHECK_INTERVAL 70

class MM_RealtimeMarkingScheme : public MM_SegregatedMarkingScheme
{
	/*
	 * Data members
	 */
public:
protected:
private:
	MM_RealtimeGC *_realtimeGC;  /**< The GC that this markingScheme is associated*/
	MM_Scheduler *_scheduler;
	J9JavaVM *_javaVM;  /**< The current JVM*/
	MM_GCExtensions *_gcExtensions;

	/*
	 * Function members
	 */
public:
	static MM_RealtimeMarkingScheme *newInstance(MM_EnvironmentBase *env, MM_RealtimeGC *realtimeGC);
	void kill(MM_EnvironmentBase *env);

	void markLiveObjects(MM_EnvironmentRealtime *env);

	MMINLINE bool isScanned(omrobjectptr_t objectPtr)
	{
		return isMarked((omrobjectptr_t)((1 << J9VMGC_SIZECLASSES_LOG_SMALLEST) + (uintptr_t)objectPtr));
	}

	MMINLINE void unmark(omrobjectptr_t objPtr)
	{
		_markMap->clearBit(objPtr);
	}

	MMINLINE void mark(omrobjectptr_t objectPtr)
	{
		_markMap->setBit(objectPtr);
	}

	MMINLINE void markAtomic(omrobjectptr_t objectPtr)
	{
		_markMap->atomicSetBit(objectPtr);
	}

	MMINLINE void setScanAtomic(omrobjectptr_t objectPtr)
	{
		_markMap->atomicSetBit((omrobjectptr_t)((1 << J9VMGC_SIZECLASSES_LOG_SMALLEST) + (uintptr_t)objectPtr));
	}

	MMINLINE bool 
	markObject(MM_EnvironmentRealtime *env, J9Object *objectPtr, bool leafType = false)
	{	
		if (objectPtr == NULL) {
			return false;
		}

		if (isMarked(objectPtr)) {
	 		return false;
		}
		
		if (!_markMap->atomicSetBit(objectPtr)) {
			return false;
		}
		if (!leafType) {
			env->getWorkStack()->push(env, (void *)OBJECT_TO_ITEM(objectPtr));
		}

		return true;
	}
	
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	MMINLINE void markClassOfObject(MM_EnvironmentRealtime *env, J9Object *objectPtr)
	{
		markClassNoCheck(env, J9GC_J9OBJECT_CLAZZ(objectPtr));
	}

	MMINLINE bool markClassNoCheck(MM_EnvironmentRealtime *env, J9Class *clazz)
	{
		bool result = false;
		if (J9_ARE_ANY_BITS_SET(J9CLASS_EXTENDED_FLAGS(clazz), J9ClassIsAnonymous)) {
			/*
			 * If class is anonymous it's classloader will not be rescanned
			 * so class object should be marked directly
			 */
			result = markObject(env, clazz->classObject);
		} else {
			result = markObject(env, clazz->classLoader->classLoaderObject);
		}
		return result;
	}

#endif
	bool markClass(MM_EnvironmentRealtime *env, J9Class *objectPtr);
	bool incrementalConsumeQueue(MM_EnvironmentRealtime *env, UDATA maxCount);
	UDATA scanPointerArraylet(MM_EnvironmentRealtime *env, fj9object_t *arraylet);
	UDATA scanPointerRange(MM_EnvironmentRealtime *env, fj9object_t *startScanPtr, fj9object_t *endScanPtr);
	UDATA scanObject(MM_EnvironmentRealtime *env, J9Object *objectPtr);
	UDATA scanMixedObject(MM_EnvironmentRealtime *env, J9Object *objectPtr);
	UDATA scanPointerArrayObject(MM_EnvironmentRealtime *env, J9IndexableObject *objectPtr);
	UDATA scanReferenceMixedObject(MM_EnvironmentRealtime *env, J9Object *objectPtr);
#if defined(J9VM_GC_FINALIZATION)
	void scanUnfinalizedObjects(MM_EnvironmentRealtime *env);
#endif /* J9VM_GC_FINALIZATION */

	/**
	 * Wraps the MM_RootScanner::scanOwnableSynchronizerObjects method to disable yielding during the scan
	 * then yield after scanning.
	 * @see MM_RootScanner::scanOwnableSynchronizerObjects()
	 */
	void scanOwnableSynchronizerObjects(MM_EnvironmentRealtime *env);

	/**
	 * Create a MM_RealtimeMarkingScheme object
	 */
	MM_RealtimeMarkingScheme(MM_EnvironmentBase *env, MM_RealtimeGC *realtimeGC)
		: MM_SegregatedMarkingScheme(env)
		, _realtimeGC(realtimeGC)
		, _scheduler(NULL)
		, _javaVM(NULL)
		, _gcExtensions(NULL)
	{
		_typeId = __FUNCTION__;
	}
protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);
private:
	void markRoots(MM_EnvironmentRealtime *env, MM_RealtimeMarkingSchemeRootMarker *rootScanner);
	/**
	 * Called by the root scanner to scan all WeakReference objects discovered by the mark phase,
	 * clearing and enqueuing them if necessary.
	 * @param env[in] the current thread
	 */
	void scanWeakReferenceObjects(MM_EnvironmentRealtime *env);
	/**
	 * Process the list of reference objects recorded in the specified list.
	 * References with unmarked referents are cleared and optionally enqueued.
	 * SoftReferences have their ages incremented.
	 * @param env[in] the current thread
	 * @param region[in] the region all the objects in the list belong to
	 * @param headOfList[in] the first object in the linked list
	 */
	void processReferenceList(MM_EnvironmentRealtime *env, MM_HeapRegionDescriptorRealtime* region, J9Object* headOfList, MM_ReferenceStats *referenceStats);

	/**
	 * Called by the root scanner to scan all SoftReference objects discovered by the mark phase,
	 * clearing and enqueuing them if necessary.
	 * @param env[in] the current thread
	 */
	void scanSoftReferenceObjects(MM_EnvironmentRealtime *env);

	/**
	 * Called by the root scanner to scan all PhantomReference objects discovered by the mark phase,
	 * clearing and enqueuing them if necessary.
	 * @param env[in] the current thread
	 */
	void scanPhantomReferenceObjects(MM_EnvironmentRealtime *env);

	friend class MM_RealtimeMarkingSchemeRootClearer;
};

#endif /* REALTIMEMARKINGSCHEME_HPP_ */
