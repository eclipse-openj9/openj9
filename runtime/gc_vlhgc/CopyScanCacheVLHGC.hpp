
/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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

/**
 * @file
 * @ingroup GC_Modron_Base
 */

#if !defined(COPYSCANCACHEVLHGC_HPP_)
#define COPYSCANCACHEVLHGC_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "j9comp.h"
#include "modron.h"

#include "CopyScanCache.hpp"

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Base
 */
class MM_CopyScanCacheVLHGC : public MM_CopyScanCache
{
	/* Data Members */
private:
protected:
public:
	GC_ObjectIteratorState _objectIteratorState; /**< the scan state of the partially scanned object */
	uintptr_t _compactGroup; /**< The compact group this cache belongs to */
	double _allocationAgeSizeProduct; /**< sum of (age * size) products for each object copied to this copy cache */
	uintptr_t _objectSize;   /**< sum of objects sizes copied to this copy cache */
	uint64_t _lowerAgeBound; /**< lowest possible age of any object in this copy cache */
	uint64_t _upperAgeBound; /**< highest possible age of any object in this copy cache */
	uintptr_t _arraySplitIndex; /**< The index within the array in scanCurrent to start scanning from (meaningful is OMR_COPYSCAN_CACHE_TYPE_SPLIT_ARRAY is set) */

	/* Members Function */
private:
protected:
public:
	/**
	 * Clears the flag on the cache which denotes that is represents a split array.
	 */
	MMINLINE void clearSplitArray()
	{
		flags &= ~OMR_COPYSCAN_CACHE_TYPE_SPLIT_ARRAY;
		_arraySplitIndex = 0;
	}

	/**
	 * Determine whether the receiver represents a split array. 
	 * If so, the array object may be found in scanCurrent and the index in _arraySplitIndex.
	 * @return whether the receiver represents a split array
	 */	
	MMINLINE bool isSplitArray() const
	{
		return OMR_COPYSCAN_CACHE_TYPE_SPLIT_ARRAY == (flags & OMR_COPYSCAN_CACHE_TYPE_SPLIT_ARRAY);
	}

	/**
	 * Create a CopyScanCacheVLHGC object.
	 */	
	MM_CopyScanCacheVLHGC()
		: MM_CopyScanCache()
		, _compactGroup(UDATA_MAX)
		, _allocationAgeSizeProduct(0.0)
		, _objectSize(0)
		, _lowerAgeBound(U_64_MAX)
		, _upperAgeBound(0)
		, _arraySplitIndex(0)
	{}
};

#endif /* COPYSCANCACHEVLHGC_HPP_ */
