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

#if !defined(REALTIMEMARKINGSCHEME_HPP_)
#define REALTIMEMARKINGSCHEME_HPP_

#include "omr.h"

#include "Base.hpp"
#include "EnvironmentRealtime.hpp"
#include "GCExtensionsBase.hpp"
#include "MarkMap.hpp"

#include "SegregatedMarkingScheme.hpp"

class MM_RealtimeGC;
class MM_RealtimeRootScanner;
class MM_Scheduler;

#define ITEM_IS_ARRAYLET 0x1
#define IS_ITEM_OBJECT(item) ((item & ITEM_IS_ARRAYLET) == 0x0)
#define IS_ITEM_ARRAYLET(item) ((item & ITEM_IS_ARRAYLET) == ITEM_IS_ARRAYLET)
#define ITEM_TO_OBJECT(item) ((omrobjectptr_t)(((uintptr_t)item) & (~ITEM_IS_ARRAYLET)))
#define ITEM_TO_ARRAYLET(item) ((fomrobject_t *)(((uintptr_t)item) & (~ITEM_IS_ARRAYLET)))
#define ITEM_TO_UDATAP(item) ((uintptr_t *)(((uintptr_t)item) & (~ITEM_IS_ARRAYLET)))
#define OBJECT_TO_ITEM(obj) ((uintptr_t) obj)
#define ARRAYLET_TO_ITEM(arraylet) (((uintptr_t) arraylet) | ITEM_IS_ARRAYLET)

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

	/*
	 * Function members
	 */
public:
	static MM_RealtimeMarkingScheme *newInstance(MM_EnvironmentBase *env, MM_RealtimeGC *realtimeGC);
	void kill(MM_EnvironmentBase *env);

	void markLiveObjectsInit(MM_EnvironmentBase *env, bool initMarkMap);
	void markLiveObjectsRoots(MM_EnvironmentBase *env);
	void markLiveObjectsScan(MM_EnvironmentBase *env);
	void markLiveObjectsComplete(MM_EnvironmentBase *env);

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
	markObject(MM_EnvironmentRealtime *env, omrobjectptr_t objectPtr, bool leafType = false)
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

	bool incrementalCompleteScan(MM_EnvironmentRealtime *env, uintptr_t maxCount);

	/**
	 * Create a MM_RealtimeMarkingScheme object
	 */
	MM_RealtimeMarkingScheme(MM_EnvironmentBase *env, MM_RealtimeGC *realtimeGC)
		: MM_SegregatedMarkingScheme(env)
		, _realtimeGC(realtimeGC)
		, _scheduler(NULL)
	{
		_typeId = __FUNCTION__;
	}
protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

	/*
	 * Friends
	 */
	friend class MM_MetronomeDelegate;
};

#endif /* REALTIMEMARKINGSCHEME_HPP_ */
