
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

/**
 * @file
 * @ingroup GC_CheckEngine
 */

#if !defined(CHECKENGINE_HPP_)
#define CHECKENGINE_HPP_

#include "j9.h"
#include "j9cfg.h"

#include "Base.hpp"
#include "CheckBase.hpp"
#include "CheckReporter.hpp"
#include "CheckElement.hpp"
#include "HeapIteratorAPI.h"
#include "MemorySpace.hpp"

class GC_CheckCycle;
class GC_FinalizeList;
class GC_ScanFormatter;
class GC_VMThreadIterator;
class MM_SublistPool;
class MM_SublistPuddle;

/**
 * Check GC structures.
 * Validate the object and class heap.  Then validate pointers, flags and other
 * structures used by garbage collection.
 * @ingroup GC_CheckEngine
 */
class GC_CheckEngine : public MM_Base
{
/*
 * Data members
 */
private:
	J9JavaVM *_javaVM;
	J9PortLibrary *_portLibrary;
	GC_CheckReporter *_reporter; /**< Device to report errors to */
	GC_CheckCycle *_cycle; /**< Object holding the details of the current check cycle */
	GC_Check *_currentCheck; /**< The check current being run */
	GC_CheckElement _lastHeapObject1; /**<  The last object visited */
	GC_CheckElement _lastHeapObject2; /**<  The last object visited */
	GC_CheckElement _lastHeapObject3; /**<  The last object visited */
	J9MM_IterateRegionDescriptor _regionDesc; /**< The last region found for an object */
	enum { CLASS_CACHE_SIZE = 19 }; /**< The size of the checked class caches (a prime number) */ 
	J9Class *_checkedClassCache[CLASS_CACHE_SIZE]; /**< A cache of recently checked classes, guaranteed not to be undead */ 
	J9Class *_checkedClassCacheAllowUndead[CLASS_CACHE_SIZE]; /**< A cache of recently checked classes, including checked undead classes */ 
	enum { OBJECT_CACHE_SIZE = 61 }; /**< The size of the checked object caches (a prime number) */ 
	J9Object *_checkedObjectCache[OBJECT_CACHE_SIZE]; /**< A cache of recently checked objects */ 

	#define UNINITIALIZED_SIZE_FOR_OWNABLESYNCHRONIER ((UDATA)-1)
	UDATA	_ownableSynchronizerObjectCountOnList; /**< the count of ownableSynchronizerObjects on the ownableSynchronizerLists, =UNINITIALIZED_SIZE_FOR_OWNABLESYNCHRONIER indicates that the count has not been calculated */
	UDATA	_ownableSynchronizerObjectCountOnHeap; /**< the count of ownableSynchronizerObjects on the heap, =UNINITIALIZED_SIZE_FOR_OWNABLESYNCHRONIER indicates that the count has not been calculated */
	
protected:

public:
#if defined(J9VM_GC_MODRON_SCAVENGER)	
	bool _scavengerBackout; /**< flag for whether a scavenge backout occurred or not */
	bool _rsOverflowState; /**< flag for whether remembered set is in overflow state or not */
#endif /* J9VM_GC_MODRON_SCAVENGER */

/*
 * Function members
 */
private:
	bool isPointerInSegment(void *pointer, J9MemorySegment *segment);
	bool isObjectOnStack(J9Object *objectPtr, J9JavaStack *stack);
	J9MemorySegment* findSegmentForClass(J9JavaVM *javaVM, J9Class *clazz);
	
	bool findRegionForPointer(J9JavaVM* javaVM, void* pointer, J9MM_IterateRegionDescriptor* regionDescOutput);
	UDATA checkJ9ObjectPointer(J9JavaVM *javaVM, J9Object *objectPtr, J9Object **newObjectPtr, J9MM_IterateRegionDescriptor *regionDesc);
	UDATA checkJ9Object(J9JavaVM *javaVM, J9Object* objectPtr, J9MM_IterateRegionDescriptor *regionDesc, UDATA checkFlags);
	UDATA checkObjectIndirect(J9JavaVM *javaVM, J9Object *objectPtr);

	UDATA checkJ9Class(J9JavaVM *javaVM, J9Class *clazz, J9MemorySegment *segment, UDATA checkFlags);
	UDATA checkJ9ClassHeader(J9JavaVM *javaVM, J9Class *clazz);
	UDATA checkStackObject(J9JavaVM *javaVM, J9Object *objectPtr);
	UDATA checkJ9ClassIsNotUnloaded(J9JavaVM *javaVM, J9Class *clazz);

	/**
	 * Check correctness of class ramStatics.
	 * Generate messages about discovered problems.
	 * @param vm - javaVM
	 * @param clazz - class to scan
	 * @return successful operation complete code or error code
	 */
	UDATA checkClassStatics(J9JavaVM* vm, J9Class* clazz);

	/**
	 * Clear the cache of classes and objects which have already been checked in this cycle.
	 */
	void clearCheckedCache();
	
	bool initialize(void);

protected:

public:
	MMINLINE J9JavaVM *getJavaVM() { return _javaVM; };

	void clearPreviousObjects();
	void pushPreviousObject(J9Object *objectPtr);
	void pushPreviousClass(J9Class *clazz);

	void clearCountsForOwnableSynchronizerObjects();
	bool verifyOwnableSynchronizerObjectCounts();
	MMINLINE void initializeOwnableSynchronizerCountOnList() { _ownableSynchronizerObjectCountOnList = 0; };
	MMINLINE void initializeOwnableSynchronizerCountOnHeap() { _ownableSynchronizerObjectCountOnHeap = 0; };

	UDATA checkObjectHeap(J9JavaVM *javaVM, J9MM_IterateObjectDescriptor *objectDesc, J9MM_IterateRegionDescriptor *regionDesc);
	UDATA checkSlotObjectHeap(J9JavaVM *javaVM, J9Object *objectPtr, fj9object_t *objectIndirect, J9MM_IterateRegionDescriptor *regionDesc, J9Object *objectIndirectBase);
	UDATA checkSlotVMThread(J9JavaVM *javaVM, J9Object **objectIndirect, void *objectIndirectBase, UDATA objectType, GC_VMThreadIterator *vmthreadIterator);
	UDATA checkSlotStack(J9JavaVM *javaVM, J9Object **objectIndirect, J9VMThread *vmThread, const void *stackLocation);
	UDATA checkSlotRememberedSet(J9JavaVM *javaVM, J9Object **objectIndirect, MM_SublistPuddle *puddle);
	UDATA checkSlotUnfinalizedList(J9JavaVM *javaVM, J9Object **objectIndirect, MM_UnfinalizedObjectList *currentList);
	
	/**
	 * Verify a ownableSynchronizer object list slot.
	 *
	 * @param objectIndirect the slot to be verified
	 * @param currentList the ownableSynchronizerObjectList which contains the slot
	 *
	 * @return #J9MODRON_SLOT_ITERATOR_OK
	 */	
	UDATA checkSlotOwnableSynchronizerList(J9JavaVM *javaVM, J9Object **objectIndirect, MM_OwnableSynchronizerObjectList *currentList);
	UDATA checkSlotFinalizableList(J9JavaVM *javaVM, J9Object **objectIndirect, GC_FinalizeListManager *listManager);
	UDATA checkSlotPool(J9JavaVM *javaVM, J9Object **objectIndirect, void *objectIndirectBase);
	UDATA checkClassHeap(J9JavaVM *javaVM, J9Class *clazz, J9MemorySegment *segment);
	UDATA checkJ9ClassPointer(J9JavaVM *javaVM, J9Class *clazz, bool allowUndead = false);
	
	
	static GC_CheckEngine *newInstance(J9JavaVM *javaVM, GC_CheckReporter *reporter);
	void kill();

	void startCheckCycle(J9JavaVM *javaVM, GC_CheckCycle *checkCycle);
	void endCheckCycle(J9JavaVM *javaVM);
	void startNewCheck(GC_Check *check);	
	bool isStackDumpAlwaysDisplayed();
	void copyRegionDescription(J9MM_IterateRegionDescriptor* from, J9MM_IterateRegionDescriptor* to);
	void clearRegionDescription(J9MM_IterateRegionDescriptor* toClear);
	
	/**
	 * Set an upper limit on how many errors are reported.
	 * Any errors that are encountered after this limit is reached will not be reported.
	 * @param count the maximum number of errors to report
	 */
	MMINLINE void setMaxErrorsToReport(UDATA count) { _reporter->setMaxErrorsToReport(count); };
	
	GC_CheckEngine(J9JavaVM *javaVM, GC_CheckReporter *reporter)
		: MM_Base()
		, _javaVM(javaVM)
		, _portLibrary(javaVM->portLibrary)
		, _reporter(reporter)
		, _cycle(NULL)
		, _currentCheck(NULL)
		, _lastHeapObject1()
		, _lastHeapObject2()
		, _lastHeapObject3()
		, _ownableSynchronizerObjectCountOnList(UNINITIALIZED_SIZE_FOR_OWNABLESYNCHRONIER)
		, _ownableSynchronizerObjectCountOnHeap(UNINITIALIZED_SIZE_FOR_OWNABLESYNCHRONIER)
#if defined(J9VM_GC_MODRON_SCAVENGER)	
		, _scavengerBackout(false)
		, _rsOverflowState(false)
#endif /* J9VM_GC_MODRON_SCAVENGER */
	{}
};

#endif /* CHECKENGINE_HPP_ */
