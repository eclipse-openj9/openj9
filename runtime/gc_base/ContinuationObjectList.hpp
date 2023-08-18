/*******************************************************************************
 * Copyright IBM Corp. and others 2022
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

#ifndef CONTINUATIONOBJECTLIST_HPP_
#define CONTINUATIONOBJECTLIST_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"

#include "BaseNonVirtual.hpp"
#include "ObjectAccessBarrier.hpp"

class MM_EnvironmentBase;

/**
 * A global list of Continuation objects.
 */
class MM_ContinuationObjectList : public MM_BaseNonVirtual
{
/* data members */
private:
	volatile j9object_t _head; /**< the head of the linked list of Continuation objects */
	j9object_t _priorHead; /**< the head of the linked list before Continuation object processing */
	MM_ContinuationObjectList *_nextList; /**< a pointer to the next Continuation list in the global linked list of lists */
	MM_ContinuationObjectList *_previousList; /**< a pointer to the previous Continuation list in the global linked list of lists */
#if defined(J9VM_GC_VLHGC)
	uintptr_t _objectCount; /**< the number of objects in the list */
#endif /* defined(J9VM_GC_VLHGC) */
protected:
public:

/* function members */
private:
protected:
public:
	/**
	 * Allocate and initialize an array of MM_ContinuationObjectList instances, resembling the functionality of operator new[].
	 *
	 * @param env the current thread
	 * @param arrayElementsTotal the number of lists to create
	 * @param listsToCopy existing MM_ContinuationObjectList array to use to construct a new array of lists
	 * @param arrayElementsToCopy the size of the list array to be copied
	 *
	 * @return a pointer to the first list in the array, or NULL if we failed to allocate/init the array
	 */
	static MM_ContinuationObjectList *newInstanceArray(MM_EnvironmentBase *env, uintptr_t arrayElementsTotal, MM_ContinuationObjectList *listsToCopy, uintptr_t arrayElementsToCopy);
	bool initialize(MM_EnvironmentBase *env);

	/**
	 * Add the specified linked list of objects to the buffer.
	 * The objects are expected to be in a NULL terminated linked
	 * list, starting from head and end at tail.
	 * This call is thread safe.
	 *
	 * @param env[in] the current thread
	 * @param head[in] the first object in the list to add
	 * @param tail[in] the last object in the list to add
	 */
	void addAll(MM_EnvironmentBase* env, j9object_t head, j9object_t tail);

	/**
	 * Fetch the head of the linked list, as it appeared at the beginning of Continuation object processing.
	 * @return the head object, or NULL if the list is empty
	 */
	MMINLINE j9object_t getHeadOfList() { return _head; }

	/**
	 * Move the list to the prior list and reset the current list to empty.
	 * The prior list may be examined with wasEmpty() and getPriorList().
	 */
	void startProcessing() {
		_priorHead = _head;
		_head = NULL;
#if defined(J9VM_GC_VLHGC)
		clearObjectCount();
#endif /* defined(J9VM_GC_VLHGC) */
	}

	/**
	 * copy the list to the prior list for backup, can be restored via calling backoutList()
	 */
	MMINLINE void backupList() {
		_priorHead = _head;
	}

	/**
	 * restore the list from the prior list, and reset the prior list to empty.
	 */
	MMINLINE void backoutList() {
		_head = _priorHead;
		_priorHead = NULL;
	}

	/**
	 * Determine if the list was empty at the beginning of Continuation object processing.
	 * @return true if the list was empty, false otherwise
	 */
	bool wasEmpty() { return NULL == _priorHead; }

	bool isEmpty() { return NULL == _head; }
	/**
	 * Fetch the head of the linked list, as it appeared at the beginning of Continuation object processing.
	 * @return the head object, or NULL if the list is empty
	 */
	j9object_t getPriorList() { return _priorHead; }

	/**
	 * Return a pointer to the next Continuation object list in the global linked list of lists, or NULL if this is the last list.
	 * @return the next list in the global list
	 */
	MMINLINE MM_ContinuationObjectList* getNextList() { return _nextList; }

	/**
	 * Set the link to the next Continuation object list in the global linked list of lists.
	 * @param nextList[in] the next list, or NULL
	 */
	void setNextList(MM_ContinuationObjectList* nextList) { _nextList = nextList; }

	/**
	 * Return a pointer to the previous Continuation object list in the global linked list of lists, or NULL if this is the first list.
	 * @return the next list in the global list
	 */
	MMINLINE MM_ContinuationObjectList* getPreviousList() { return _previousList; }

	/**
	 * Set the link to the previous Continuation object list in the global linked list of lists.
	 * @param nextList[in] the next list, or NULL
	 */
	void setPreviousList(MM_ContinuationObjectList* previousList) { _previousList = previousList; }

	/**
	 * Construct a new list.
	 */
	MM_ContinuationObjectList();

#if defined(J9VM_GC_VLHGC)
	MMINLINE uintptr_t getObjectCount() { return _objectCount; }
	MMINLINE void clearObjectCount() { _objectCount = 0; }
	MMINLINE void incrementObjectCount(uintptr_t count)
	{
		MM_AtomicOperations::add((volatile uintptr_t *)&_objectCount, count);
	}
#endif /* defined(J9VM_GC_VLHGC) */

};
#endif /* CONTINUATIONOBJECTLIST_HPP_ */
