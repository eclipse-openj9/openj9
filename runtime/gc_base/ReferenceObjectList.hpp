
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

#ifndef REFERENCEOBJECTLIST_HPP_
#define REFERENCEOBJECTLIST_HPP_

#include "j9.h"
#include "j9cfg.h"

#include "BaseNonVirtual.hpp"

class MM_EnvironmentBase;

/**
 * A global list of reference objects.
 */
class MM_ReferenceObjectList : public MM_BaseNonVirtual
{
/* data members */
private:
	volatile j9object_t _weakHead; 		/**< the head of the linked list of weak reference objects */
	volatile j9object_t _softHead; 		/**< the head of the linked list of soft reference objects */
	volatile j9object_t _phantomHead; 	/**< the head of the linked list of phantom reference objects */
	j9object_t _priorWeakHead; 			/**< the head of the weak linked list before reference object processing */
	j9object_t _priorSoftHead; 			/**< the head of the soft linked list before reference object processing */
	j9object_t _priorPhantomHead; 		/**< the head of the phantom linked list before reference object processing */
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
	 * @param referenceObjectType the type of objects being added (weak/soft/phantom)
	 * @param head[in] the first object in the list to add
	 * @param tail[in] the last object in the list to add
	 */
	void addAll(MM_EnvironmentBase* env, UDATA referenceObjectType, j9object_t head, j9object_t tail);

	/**
	 * Move the list to the prior list and reset the current list to empty.
	 * The prior list may be examined with wasEmpty() and getPriorList().
	 */
	void startWeakReferenceProcessing() {
		_priorWeakHead = _weakHead;
		_weakHead = NULL; 
	}

	/**
	 * Move the list to the prior list and reset the current list to empty.
	 * The prior list may be examined with wasEmpty() and getPriorList().
	 */
	void startSoftReferenceProcessing() {
		_priorSoftHead = _softHead;
		_softHead = NULL; 
	}

	/**
	 * Move the list to the prior list and reset the current list to empty.
	 * The prior list may be examined with wasEmpty() and getPriorList().
	 */
	void startPhantomReferenceProcessing() {
		_priorPhantomHead = _phantomHead;
		_phantomHead = NULL; 
	}
	
	/**
	 * Determine if the list was empty at the beginning of reference object processing.
	 * @return true if the list was empty, false otherwise
	 */
	bool wasWeakListEmpty() { return NULL == _priorWeakHead; }

	/**
	 * Determine if the list was empty at the beginning of reference object processing.
	 * @return true if the list was empty, false otherwise
	 */
	bool wasSoftListEmpty() { return NULL == _priorSoftHead; }

	/**
	 * Determine if the list was empty at the beginning of reference object processing.
	 * @return true if the list was empty, false otherwise
	 */
	bool wasPhantomListEmpty() { return NULL == _priorPhantomHead; }

	bool isWeakListEmpty() { return NULL == _weakHead; }
	bool isSoftListEmpty() { return NULL == _softHead; }
	bool isPhantomListEmpty() { return NULL == _phantomHead; }

	/**
	 * Fetch the head of the linked list, as it appeared at the beginning of reference object processing.
	 * @return the head object, or NULL if the list is empty
	 */
	j9object_t getPriorWeakList() { return _priorWeakHead; }

	/**
	 * Fetch the head of the linked list, as it appeared at the beginning of reference object processing.
	 * @return the head object, or NULL if the list is empty
	 */
	j9object_t getPriorSoftList() { return _priorSoftHead; }

	/**
	 * Fetch the head of the linked list, as it appeared at the beginning of reference object processing.
	 * @return the head object, or NULL if the list is empty
	 */
	j9object_t getPriorPhantomList() { return _priorPhantomHead; }

	/**
	 * Clear all of the prior lists.
	 * After this call, was{Weak|Soft|Phantom}ListEmpty() will return true.
	 */
	void resetPriorLists() {
		_priorWeakHead = NULL;
		_priorSoftHead = NULL;
		_priorPhantomHead = NULL;
	}
	
	/**
	 * Clear all lists, head or prior.
	 * After this call, was{Weak|Soft|Phantom}ListEmpty() will return true.
	 */
	void resetLists() {
		resetPriorLists();
		_weakHead = NULL;
		_softHead = NULL;
		_phantomHead = NULL;
	}

	/**
	 * Construct a new list.
	 */
	MM_ReferenceObjectList();
};

#endif /* REFERENCEOBJECTLIST_HPP_ */
