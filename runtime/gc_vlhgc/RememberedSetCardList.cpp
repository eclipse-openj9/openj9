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

#include "HeapRegionManager.hpp"
#include "RememberedSetCardList.hpp"
#include "VMThreadListIterator.hpp"
#include "InterRegionRememberedSet.hpp"

bool
MM_RememberedSetCardList::initialize(MM_EnvironmentVLHGC *env, UDATA index)
{
	_index = index;
	MM_RememberedSetCardBucket *bucket = &(env->_rememberedSetCardBucketPool[_index]);
	new(bucket) MM_RememberedSetCardBucket();
	bucket->initialize(env, this, _bucketListHead);
	_bucketListHead = bucket;

	return true;
}

void
MM_RememberedSetCardList::tearDown(MM_GCExtensions *extensions)
{
	MM_RememberedSetCardBucket *currentBucket = _bucketListHead;
	while (NULL != currentBucket) {
		currentBucket->tearDown(extensions);
		currentBucket = currentBucket->_next;
	}
}

bool
MM_RememberedSetCardList::isEmpty(MM_EnvironmentVLHGC *env)
{
	bool empty = true;

	if (TRUE == _overflowed) {
		empty = false;
	} else {
		if (0 != _bufferCount) {
			empty = false;
		} else {
			MM_RememberedSetCardBucket *currentBucket = _bucketListHead;
			while (NULL != currentBucket) {
				if (!currentBucket->isEmpty(env)) {
					empty = false;
					break;
				}
				currentBucket = currentBucket->_next;
			}
		}

		/* If it is empty, the size must be 0 */
		Assert_MM_true(empty == (0 == getSize(env)));
	}

	return empty;
}


UDATA
MM_RememberedSetCardList::getSize(MM_EnvironmentVLHGC *env)
{
	UDATA size = 0;
	UDATA checkBufferCount = 0; /* used to validate the consistency of _bufferCount */
	
	MM_RememberedSetCardBucket *currentBucket = _bucketListHead;
	while (NULL != currentBucket) {
		size += currentBucket->getSize(env);
		checkBufferCount += currentBucket->getBufferCount(env);
		currentBucket = currentBucket->_next;
	}

	Assert_MM_true(_bufferCount == checkBufferCount);
	
	return size;
}

void
MM_RememberedSetCardList::releaseBuffers(MM_EnvironmentVLHGC *env)
{
	if (0 != _bufferCount) {
		MM_RememberedSetCardBucket *currentBucket = _bucketListHead;
		while (NULL != currentBucket) {
			currentBucket->localReleaseBuffers(env);
			currentBucket = currentBucket->_next;
		}
	}

	Assert_MM_true(0 == _bufferCount);
}

void
MM_RememberedSetCardList::releaseBuffersForCurrentThread(MM_EnvironmentVLHGC *env)
{
	MM_RememberedSetCardBucket *currentBucket = mapToBucket(env);
	currentBucket->globalReleaseBuffers(env);
}

void
MM_RememberedSetCardList::clear(MM_EnvironmentVLHGC *env)
{
	releaseBuffers(env);
	_overflowed = FALSE;
	_stable = false;
}

void
MM_RememberedSetCardList::add(MM_EnvironmentVLHGC *env, J9Object *object)
{
	MM_InterRegionRememberedSet *interRegionRememberedSet = MM_GCExtensions::getExtensions(env)->interRegionRememberedSet;
	MM_RememberedSetCard card = interRegionRememberedSet->getRememberedSetCardFromJ9Object(object);
	MM_RememberedSetCardBucket *bucket = mapToBucket(env);
	bucket->add(env, card);

}

bool
MM_RememberedSetCardList::isRemembered(MM_EnvironmentVLHGC *env, MM_RememberedSetCard card)
{
	Assert_MM_true(FALSE == _overflowed);
	
	MM_RememberedSetCardBucket *currentBucket = _bucketListHead;
	while (NULL != currentBucket) {
		if (currentBucket->isRemembered(env, card)) {
			return true;
		}
		currentBucket = currentBucket->_next;
	}

	return false;
}

bool
MM_RememberedSetCardList::isRemembered(MM_EnvironmentVLHGC *env, J9Object *object)
{
	MM_InterRegionRememberedSet *interRegionRememberedSet = MM_GCExtensions::getExtensions(env)->interRegionRememberedSet;
	return isRemembered(env, interRegionRememberedSet->getRememberedSetCardFromJ9Object(object));
}

void
MM_RememberedSetCardList::compact(MM_EnvironmentVLHGC *env)
{
	Assert_MM_true(FALSE == _overflowed);
	UDATA checkBufferCount = 0; /* used to validate the consistency of _bufferCount */

	MM_RememberedSetCardBucket *currentBucket = _bucketListHead;
	while (NULL != currentBucket) {
		currentBucket->compact(env);
		checkBufferCount += currentBucket->getBufferCount(env);
		currentBucket = currentBucket->_next;
	}
	
	Assert_MM_true(_bufferCount == checkBufferCount);
}
