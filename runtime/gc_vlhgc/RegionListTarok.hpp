
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

/**
 * @file
 * @ingroup GC_Modron_Base
 */

#if !defined(REGIONLISTTAROK_HPP_)
#define REGIONLISTTAROK_HPP_

#include "j9.h"
#include "omrthread.h"
#include "modron.h"

#include "BaseNonVirtual.hpp"
#include "HeapRegionDescriptorVLHGC.hpp"

class MM_HeapRegionManager;
class MM_HeapRegionDescriptorVLHGC;
class MM_EnvironmentBase;

/**
 * A region list which manages a single list of MM_HeapRegionDescriptorVLHGC regions, connected via their _nextInList and _previousInList links.
 * NOTE: This list is NOT THREAD SAFE and not locked so any multi-threaded users of this structure must be externally serialized.
 * 
 * @ingroup GC_Modron_Tarok
 */
class MM_RegionListTarok : public MM_BaseNonVirtual
{
private:
	MM_HeapRegionDescriptorVLHGC *_regions;	/**< The list of regions, stitched together with _nextInList links */
	UDATA _listSize;	/**< The number of regions currently held in this list */
protected:
public:	
	
private:
protected:
public:
	/**
	 * Inserts a region into the list.  In the current implementation, the new region is added to the beginning of the list.
	 * @param region[in] The region which will be added to the list at its head
	 */
	void insertRegion(MM_HeapRegionDescriptorVLHGC *region);
	/**
	 * Removes a region from the list.  The region will be modified so that its next and previous pointers both point to NULL.
	 * @param region[in] The region which will be removed from this list
	 */
	void removeRegion(MM_HeapRegionDescriptorVLHGC *region);
	/**
	 * Returns the first region in the list (NULL if the list is empty).  The region returned is suitable for use in peekRegionAfter() for a user
	 * wishing to iterate the regions.
	 * @return The region at the head of the list or NULL if the list is empty
	 */
	MMINLINE MM_HeapRegionDescriptorVLHGC *peekFirstRegion()
	{
		return _regions;
	}
	/**
	 * Returns the next region in the list after region (NULL if region is at the end of the list).  The region returned is suitable for use in
	 * successive peekRegionAfter() calls for a user wishing to iterate the regions.
	 * Note that the behaviour of this function is undefined if the list has been changed since the region parameter was retrieved from this list
	 * or if the region is not a member of this list.
	 * @param region[in] The region from which the next region will be determined (cannot be NULL)
	 * @return The region after region or NULL if this is the end of the list
	 */
	MMINLINE MM_HeapRegionDescriptorVLHGC *peekRegionAfter(MM_HeapRegionDescriptorVLHGC *region)
	{
		return region->_allocateData._nextInList;
	}
	/**
	 * Returns the number of elements in the receiver.
	 * @return The number of regions in the list
	 */
	MMINLINE UDATA listSize() { return _listSize; }
	
	MM_RegionListTarok()
		: MM_BaseNonVirtual()
		, _regions(NULL)
		, _listSize(0)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* REGIONLISTTAROK_HPP_ */
