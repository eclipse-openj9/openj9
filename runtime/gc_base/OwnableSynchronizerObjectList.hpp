
/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#ifndef OWNABLESYNCHRONIZEROBJECTLIST_HPP_
#define OWNABLESYNCHRONIZEROBJECTLIST_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"

#include "BaseNonVirtual.hpp"
#include "ObjectAccessBarrier.hpp"

class MM_EnvironmentBase;

/**
 * A global list of OwnableSynchronizer objects.
 */
class MM_OwnableSynchronizerObjectList : public MM_BaseNonVirtual
{
/* data members */
private:
	volatile j9object_t _head; /**< the head of the linked list of OwnableSynchronizer objects */
	j9object_t _priorHead; /**< the head of the linked list before OwnableSynchronizer object processing */
	MM_OwnableSynchronizerObjectList *_nextList; /**< a pointer to the next OwnableSynchronizer list in the global linked list of lists */
	MM_OwnableSynchronizerObjectList *_previousList; /**< a pointer to the previous OwnableSynchronizer list in the global linked list of lists */
#if defined(J9VM_GC_VLHGC)
	UDATA _objectCount; /**< the number of objects in the list */
#endif /* defined(J9VM_GC_VLHGC) */
protected:
public:
	
/* function members */
private:
protected:
public:
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
	 * Fetch the head of the linked list, as it appeared at the beginning of OwnableSynchronizer object processing.
	 * @return the head object, or NULL if the list is empty
	 */
	MMINLINE j9object_t getHeadOfList() { return _head; }

	/**
	 * Move the list to the prior list and reset the current list to empty.
	 * The prior list may be examined with wasEmpty() and getPriorList().
	 */
	void startOwnableSynchronizerProcessing() {
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
	 * Determine if the list was empty at the beginning of OwnableSynchronizer object processing.
	 * @return true if the list was empty, false otherwise
	 */
	bool wasEmpty() { return NULL == _priorHead; }
	
	/**
	 * Fetch the head of the linked list, as it appeared at the beginning of OwnableSynchronizer object processing.
	 * @return the head object, or NULL if the list is empty
	 */
	j9object_t getPriorList() { return _priorHead; }
	
	/**
	 * Return a pointer to the next OwnableSynchronizer object list in the global linked list of lists, or NULL if this is the last list.
	 * @return the next list in the global list
	 */
	MMINLINE MM_OwnableSynchronizerObjectList* getNextList() { return _nextList; }
	
	/**
	 * Set the link to the next OwnableSynchronizer object list in the global linked list of lists.
	 * @param nextList[in] the next list, or NULL
	 */
	void setNextList(MM_OwnableSynchronizerObjectList* nextList) { _nextList = nextList; }

	/**
	 * Return a pointer to the previous OwnableSynchronizer object list in the global linked list of lists, or NULL if this is the first list.
	 * @return the next list in the global list
	 */
	MMINLINE MM_OwnableSynchronizerObjectList* getPreviousList() { return _previousList; }

	/**
	 * Set the link to the previous OwnableSynchronizer object list in the global linked list of lists.
	 * @param nextList[in] the next list, or NULL
	 */
	void setPreviousList(MM_OwnableSynchronizerObjectList* previousList) { _previousList = previousList; }

	/**
	 * Construct a new list.
	 */
	MM_OwnableSynchronizerObjectList();

#if defined(J9VM_GC_VLHGC)
	MMINLINE UDATA getObjectCount() { return _objectCount; }
	MMINLINE void clearObjectCount() { _objectCount = 0; }
	MMINLINE void incrementObjectCount(UDATA count)
	{
		MM_AtomicOperations::add((volatile UDATA *)&_objectCount, count);
	}
#endif /* defined(J9VM_GC_VLHGC) */

};

#endif /* OWNABLESYNCHRONIZEROBJECTLIST_HPP_ */
