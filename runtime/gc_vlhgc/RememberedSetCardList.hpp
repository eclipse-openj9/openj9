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

#if !defined(REMEMBEREDSETCARDLIST_HPP)
#define REMEMBEREDSETCARDLIST_HPP

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"
#include "ModronAssertions.h"

#include "BaseVirtual.hpp"
#include "EnvironmentVLHGC.hpp"
#include "GCExtensions.hpp"
#include "RememberedSetCardBucket.hpp"

class MM_RememberedSetCardList : public MM_BaseVirtual
{
	friend class GC_RememberedSetCardListCardIterator;
	friend class GC_RememberedSetCardListBufferIterator;
	friend class MM_InterRegionRememberedSet;
	friend class MM_RememberedSetCardBucket;

public:
protected:
private:
	MM_RememberedSetCardBucket *_bucketListHead;			/**< head of the linked list of buckets */
	UDATA _index;											/**< cached index into HeapRegionManager table */
	volatile UDATA _overflowed;								/**< list overflowed flag (TRUE, FALSE values) because it's either full and stable */
	bool _beingRebuilt;										/**< the list is being rebuilt. thus, it is incomplete and should not be used, yet */
	bool _stable;											/**< if true, list is overflowed due to region being stable */
	volatile UDATA _bufferCount;										/**< count of buffers in all buckets' lists */
	MM_RememberedSetCardList * volatile _nonEmptyOverflowedNext; 		/**< overflowed RSCL found during a GC cycle are linked into a single liked list - this is next pointer */
private:
	/**
	 * Remove an entry. This just NULLs the entry. Compaction/shifting is to be done later, explicitly.
	 */
	void removeCard(MM_RememberedSetCard *bucketCardList, UDATA index) {
		bucketCardList[index] = 0;
	}
	
	MM_RememberedSetCardBucket *mapToBucket(MM_EnvironmentVLHGC *env) {
		return &(env->_rememberedSetCardBucketPool[_index]);
	}

protected:
public:

	/**
	 * Add a card represented by the object to the list
	 *
	 * @param object  address of and object
	 * return  true if success, false if the list is overflowed
	 */
	void add(MM_EnvironmentVLHGC *env, J9Object *object);

	/**
	 * Search the list and check if this object's card is remembered.
	 * Caller assures the list is not overflowed. Not thread safe.
	 */
	bool isRemembered(MM_EnvironmentVLHGC *env, J9Object *object);
	/**
	 * Search the list and check if this card is remembered.
	 * Caller assures the list is not overflowed. Not thread safe.
	 */
	bool isRemembered(MM_EnvironmentVLHGC *env, MM_RememberedSetCard card);

	/**
	 * Check if list is overflowed
	 *
	 * return  true if at least one bucket is overflowed or if region is overflowed as stable
	 */
	bool isOverflowed() {
		return (TRUE == _overflowed);
	}

	/*
	 * return  true if overflowed as stable region
	 */
	bool isStable() {
		return _stable;
	}

	/**
	 * Check if the list is being rebuilt
	 */
	bool isBeingRebuilt() {
		return _beingRebuilt;
	}

	/**
	 * Check if the content is accurate (not overflowed and not being rebuilt)
	 */
	bool isAccurate() {
		return (FALSE == _overflowed) && !_beingRebuilt;
	}

	/**
	 * Set the list as overflowed stable
	 */
	void setAsStable() {
		_overflowed = TRUE;
		_stable = true;
	}

	/**
	 * Set the list as being rebuilt
	 */
	void setAsBeingRebuilt() {
		_beingRebuilt = true;
	}

	/**
	 * The rebuilding is complete. Reset rebuild flag. 
	 */
	void setAsRebuildingComplete() {
		_beingRebuilt = false;
	}

	/**
	 * @return true if all buckets are empty (have no cards and are not overflowed)
	 */
	bool isEmpty(MM_EnvironmentVLHGC *env);

	/**
	 * @return the size (should be used for stats purposes only)
	 */
	UDATA getSize(MM_EnvironmentVLHGC *env);

	/**
	 * @return the total count of buffers used
	 */
	UDATA getBufferCount() { return _bufferCount; }

	/**
	 * Remove NULL entries and compact the list. Not thread safe. Called only for non-overflowed lists.
	 */
	void compact(MM_EnvironmentVLHGC *env);

	/**
	 * Release buffers from all the buckets.
	 */
	void releaseBuffers(MM_EnvironmentVLHGC *env);

	/**
	 * Release buffers only for the bucked associated with current thread.
	 */
	void releaseBuffersForCurrentThread(MM_EnvironmentVLHGC *env);

	/**
	 * Empty out the list. Clear the overflow flag.
	 */
	void clear(MM_EnvironmentVLHGC *env);

	/**
	 * Initialize the list
	 */
	bool initialize(MM_EnvironmentVLHGC *env, UDATA index);

	/**
	 * Teardown the list
	 */
	virtual void tearDown(MM_GCExtensions *extensions);


	MM_RememberedSetCardList()
	  : MM_BaseVirtual()
	  , _bucketListHead(NULL)
	  , _index(0)
	  , _overflowed(FALSE)
	  , _beingRebuilt(false)
	  , _stable(false)
	  , _bufferCount(0)
	  , _nonEmptyOverflowedNext(NULL)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* REMEMBEREDSETCARDLIST_HPP */
