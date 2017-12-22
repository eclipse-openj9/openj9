
/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

#if !defined(MARKINGDELEGATE_HPP_)
#define MARKINGDELEGATE_HPP_

#include "j9nonbuilder.h"
#include "objectdescription.h"

#include "GCExtensions.hpp"
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
#include "MarkMap.hpp"
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
#include "MixedObjectScanner.hpp"
#include "ModronTypes.hpp"
#include "ReferenceObjectScanner.hpp"
#include "PointerArrayObjectScanner.hpp"

class GC_ObjectScanner;
class MM_EnvironmentBase;
class MM_HeapRegionDescriptorStandard;
class MM_MarkingScheme;
class MM_ReferenceStats;

class MM_MarkingDelegate
{
/* Data members & types */
private:
	OMR_VM *_omrVM;
	MM_GCExtensions *_extensions;
	MM_MarkingScheme *_markingScheme;
	bool _collectStringConstantsEnabled;
	bool _shouldScanUnfinalizedObjects;
	bool _shouldScanOwnableSynchronizerObjects;
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	MM_MarkMap *_markMap;							/**< This is set when dynamic class loading is enabled, NULL otherwise */
	volatile bool _anotherClassMarkPass;			/**< Used in completeClassMark for another scanning request*/
	volatile bool _anotherClassMarkLoopIteration;	/**< Used in completeClassMark for another loop iteration request (set by the Master thread)*/
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */


protected:

public:

/* Methods */

private:
	MMINLINE void clearReference(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, bool isReferenceCleared, bool referentMustBeCleared);
	MMINLINE bool getReferenceStatus(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, bool *referentMustBeMarked, bool *isReferenceCleared);
	fomrobject_t *setupReferenceObjectScanner(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, MM_MarkingSchemeScanReason reason);
	uintptr_t setupPointerArrayScanner(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, MM_MarkingSchemeScanReason reason, uintptr_t *sizeToDo, uintptr_t *slotsToDo);

protected:

public:
	MM_MarkingDelegate()
		: _omrVM(NULL)
		, _extensions(NULL)
		, _markingScheme(NULL)
		, _collectStringConstantsEnabled(false)
		, _shouldScanUnfinalizedObjects(false)
		, _shouldScanOwnableSynchronizerObjects(false)
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		, _markMap(NULL)
		, _anotherClassMarkPass(false)
		, _anotherClassMarkLoopIteration(false)
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
	{}

	/**
	 * Initialize the delegate.
	 *
	 * @param env environment for calling thread
	 * @param markingScheme the marking scheme that the delegate is bound to
	 * @return true if delegate initialized successfully
	 */
	bool initialize(MM_EnvironmentBase *env, MM_MarkingScheme *markingScheme);

	/**
	 * Delegate methods interfacing with MM_MarkingScheme.
	 */
	void workerSetupForGC(MM_EnvironmentBase *env);
	void workerCompleteGC(MM_EnvironmentBase *env);
	void workerCleanupAfterGC(MM_EnvironmentBase *env);
	void masterSetupForGC(MM_EnvironmentBase *env);
	void masterSetupForWalk(MM_EnvironmentBase *env);
	void masterCleanupAfterGC(MM_EnvironmentBase *env);
	void scanRoots(MM_EnvironmentBase *env);
	void completeMarking(MM_EnvironmentBase *env);

	MMINLINE GC_ObjectScanner *
	getObjectScanner(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, void *scannerSpace, MM_MarkingSchemeScanReason reason, uintptr_t *sizeToDo)
	{
		J9Class *clazz = J9GC_J9OBJECT_CLAZZ(objectPtr);
		/* object class must have proper eye catcher */
		Assert_MM_true((UDATA)0x99669966 == clazz->eyecatcher);

		GC_ObjectScanner *objectScanner = NULL;
		switch(_extensions->objectModel.getScanType(objectPtr)) {
		case GC_ObjectModel::SCAN_ATOMIC_MARKABLE_REFERENCE_OBJECT:
		case GC_ObjectModel::SCAN_MIXED_OBJECT:
		case GC_ObjectModel::SCAN_OWNABLESYNCHRONIZER_OBJECT:
		case GC_ObjectModel::SCAN_CLASS_OBJECT:
		case GC_ObjectModel::SCAN_CLASSLOADER_OBJECT:
			objectScanner = GC_MixedObjectScanner::newInstance(env, objectPtr, scannerSpace, 0);
			*sizeToDo = sizeof(fomrobject_t) + ((GC_MixedObjectScanner *)objectScanner)->getBytesRemaining();
			break;
		case GC_ObjectModel::SCAN_POINTER_ARRAY_OBJECT:
		{
			uintptr_t slotsToDo = 0;
			uintptr_t startIndex = setupPointerArrayScanner(env, objectPtr, reason, sizeToDo, &slotsToDo);
			objectScanner = GC_PointerArrayObjectScanner::newInstance(env, objectPtr, scannerSpace, GC_ObjectScanner::indexableObject, slotsToDo, startIndex);
			break;
		}
		case GC_ObjectModel::SCAN_REFERENCE_MIXED_OBJECT:
		{
			fomrobject_t *referentSlotPtr = setupReferenceObjectScanner(env, objectPtr, reason);
			objectScanner = GC_ReferenceObjectScanner::newInstance(env, objectPtr, referentSlotPtr, scannerSpace, 0);
			*sizeToDo = sizeof(fomrobject_t) + ((GC_ReferenceObjectScanner *)objectScanner)->getBytesRemaining();
			break;
		}
		case GC_ObjectModel::SCAN_PRIMITIVE_ARRAY_OBJECT:
			/* nothing to do */
			*sizeToDo = 0;
			break;
		default:
			Assert_MM_unreachable();
		}

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		if (isDynamicClassUnloadingEnabled()) {
			/* only mark the class the first time we scan any array, always mark class for mixed/reference objects */
			if ((NULL != objectScanner) && objectScanner->isHeadObjectScanner()) {
				/*  Note: this code cloned from MM_MarkingScheme::inlineMarkObjectNoCheck(), inaccessible here */
				if (_markMap->atomicSetBit(clazz->classObject)) {
					/* class object was previously unmarked so push it to workstack */
					env->_workStack.push(env, (void *)clazz->classObject);
					env->_markStats._objectsMarked += 1;
				}
			}
		}
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

		return objectScanner;
	}

	/**
	 * Delegate methods used by MM_MarkingSchemeRootmarker and MM_MarkingSchemeRootClearer.
	 */
	MMINLINE bool
	shouldScanUnfinalizedObjects()
	{
		return _shouldScanUnfinalizedObjects;
	}

	MMINLINE void
	shouldScanUnfinalizedObjects(bool shouldScanUnfinalizedObjects)
	{
		_shouldScanUnfinalizedObjects = shouldScanUnfinalizedObjects;
	}

	MMINLINE bool
	shouldScanOwnableSynchronizerObjects()
	{
		return _shouldScanOwnableSynchronizerObjects;
	}

	MMINLINE void
	shouldScanOwnableSynchronizerObjects(bool shouldScanOwnableSynchronizerObjects)
	{
		_shouldScanOwnableSynchronizerObjects = shouldScanOwnableSynchronizerObjects;
	}

	void scanClass(MM_EnvironmentBase *env, J9Class *clazz);

	bool processReference(MM_EnvironmentBase *env, omrobjectptr_t objectPtr);
	void processReferenceList(MM_EnvironmentBase *env, MM_HeapRegionDescriptorStandard* region, omrobjectptr_t headOfList, MM_ReferenceStats *referenceStats);

	void handleWorkPacketOverflowItem(MM_EnvironmentBase *env, omrobjectptr_t objectPtr)
	{
		if (GC_ObjectModel::SCAN_REFERENCE_MIXED_OBJECT == _extensions->objectModel.getScanType(objectPtr)) {
			/* since we popped this object from the work packet, it is our responsibility to record it in the list of reference objects */
			/* we know that the object must be in the collection set because it was on a work packet */
			/* we don't need to process cleared or enqueued references */
			processReference(env, objectPtr);
		}
	}

	/**
	 * Helper methods used by classes other than MM_MarkingScheme.
	 */
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	static void clearClassLoadersScannedFlag(MM_EnvironmentBase *env);
	MMINLINE bool isDynamicClassUnloadingEnabled() { return NULL != _markMap; }
#endif /* defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) */
};

#endif /* MARKINGDELEGATE_HPP_ */
