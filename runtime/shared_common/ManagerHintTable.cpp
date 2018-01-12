/*******************************************************************************
 * Copyright (c) 2008, 2014 IBM Corp. and others
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

#include "sharedconsts.h"

#if defined(J9SHR_CACHELET_SUPPORT)

#include "ManagerHintTable.hpp"
#include "SharedCache.hpp"
#include "ut_j9shr.h"

/**
 * @retval true success
 * @retval false failure
 */
bool
ManagerHintTable::initialize(J9PortLibrary* portlib)
{
	bool rc = true;
	if (!_hinthash) {
		_hinthash = hashTableNew(OMRPORT_FROM_J9PORT(portlib), J9_GET_CALLSITE(), 0, sizeof(HintItem), 0, 0, J9MEM_CATEGORY_CLASSES, hashFn, hashEqualFn, NULL, (void*)portlib);
		if (!_hinthash) {
			rc = false;
		}
	}

	if (!rc) {
		destroy();
	}
	return rc;
}

void
ManagerHintTable::destroy()
{
	if (_hinthash) {
		hashTableFree(_hinthash);
		_hinthash = NULL;
	}
}

/*
 * We only add hints when priming the hash table.
 * We only prime when updating the cache, with the write mutex held.
 */
bool
ManagerHintTable::addHint(J9VMThread* currentThread, UDATA hashValue, SH_CompositeCache* cachelet)
{
	void *rc;
	HintItem item(hashValue, cachelet);

	if (!_hinthash) {
		return false;
	}

	rc = hashTableAdd(_hinthash, &item);
	return (rc != NULL);
}

/* not currently used */
void
ManagerHintTable::removeHint(J9VMThread* currentThread, UDATA hashValue, SH_CompositeCache* cachelet)
{
	HintItem item(hashValue, cachelet);

	if (!_hinthash) {
		return;
	}
	hashTableRemove(_hinthash, &item);
}


SH_CompositeCache*
ManagerHintTable::findHint(J9VMThread* currentThread, UDATA hashValue)
{
	HintItem searchItem(hashValue, NULL);
	HintItem* foundItem;
	SH_CompositeCache* rc = NULL;


	if (!_hinthash) {
		return NULL;
	}
	foundItem = (HintItem*)hashTableFind(_hinthash, &searchItem);
	if (foundItem) {
		/* we should never have inserted a NULL cachelet */
		Trc_SHR_Assert_True(foundItem->_cachelet);
		rc = foundItem->_cachelet;
	}
	return rc;
}

UDATA
ManagerHintTable::hashFn(void* item_, void* userData)
{
	HintItem* item = (HintItem*)item_;
	return item->_hashValue;
}

/**
 * HashEqual function for hashtable
 * 
 * @param[in] left a match candidate from the hashtable
 * @param[in] right search key 
 * 
 * @retval 0 not equal
 * @retval non-zero equal 
 */
UDATA
ManagerHintTable::hashEqualFn(void* left, void* right, void* userData)
{
	HintItem* leftItem = (HintItem*)left;
	HintItem* rightItem = (HintItem*)right;
	UDATA rc = 0;
	
	if (leftItem->_hashValue != rightItem->_hashValue) {
		return 0;
	}

	Trc_SHR_Assert_True(leftItem->_cachelet != NULL);
	rc = ((rightItem->_cachelet == leftItem->_cachelet) || (rightItem->_cachelet == NULL));

	return rc;
}

#endif /* J9SHR_CACHELET_SUPPORT */
